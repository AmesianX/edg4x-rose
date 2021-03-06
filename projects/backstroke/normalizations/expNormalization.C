#include "expNormalization.h"
#include "utilities/utilities.h"
#include <boost/foreach.hpp>
#include <utility>
#include <boost/tuple/tuple.hpp>
#include <boost/lexical_cast.hpp>

#define foreach BOOST_FOREACH

namespace BackstrokeNorm
{
	
using namespace std;
using namespace boost;
using namespace SageBuilder;
using namespace SageInterface;
using namespace BackstrokeUtility;
using namespace BackstrokeNormUtility;
	
SgFunctionDeclaration* normalizeEvent(SgFunctionDeclaration* func_decl)
{
	func_decl = isSgFunctionDeclaration(func_decl->get_definingDeclaration());
	ROSE_ASSERT(func_decl != NULL);
	ROSE_ASSERT(func_decl->get_definition() != NULL);

	SgFunctionDefinition* def_normalized = func_decl->get_definition();

	ExtractFunctionArguments e;
        e.NormalizeTree(def_normalized);
	BackstrokeNormUtility::normalize(def_normalized->get_body());

	return func_decl;
}

namespace BackstrokeNormUtility
{

void normalizeExpressions(SgNode* node)
{
    // Note that postorder traversal is required here.
    vector<SgExpression*> exp_list = BackstrokeUtility::querySubTree<SgExpression>(node, postorder);
    foreach (SgExpression* exp, exp_list)
    {
		// If the expression is inside of a sizeof operator, we don't normalize it.
		if (isInSizeOfOp(exp)) continue;

        // First step, transform modifying expressions, like assignment, into comma expressions.
        // a += (b += c);  ==>  a += (b += c, b);
        getAndReplaceModifyingExpression(exp);

        // Then propagate those comma expressions.
        // a += (b += c, b);  ==>  b += c, a += b;
        exp = propagateCommaOpAndConditionalExp(exp);

        // Split those comma expressions.
        // b += c, a += b;  ==>  b += c; a += b;
		turnCommaOpExpIntoStmt(exp);

		// Turn conditional expression into if statement like:
		// a ? b : c;  ==>  if (a) b; else c;
		turnConditionalExpIntoStmt(exp);
    }
}


void normalize(SgNode* node)
{
	// To know what thie preprocess does, please see the definition of it.
    preprocess(node);

    //Rose_STL_Container<SgNode*> exp_list = NodeQuery::querySubTree(func->get_body(), V_SgExpression, postorder);

	// Normalize all expressions inside of this node.
	normalizeExpressions(node);

    // To improve the readibility of the transformed code.
    removeUselessBraces(node);
    removeUselessParen(node);
}

/** Get the closest basic block which contains the expression passed in. */
SgBasicBlock* getCurrentBody(SgExpression* e)
{
    SgNode* parent = e->get_parent();
    SgBasicBlock* body = isSgBasicBlock(parent);
    while (body == NULL)
    {
        parent = parent->get_parent();
        if (parent == NULL)
            return NULL;
        body = isSgBasicBlock(parent);
    }
    return body;
}

void getAndReplaceModifyingExpression(SgExpression*& e)
{
    // Avoid to bring variable reference expressions which are not used.
    // For example, 
    //     a = b = c;  ==>  a = (b = c, b);
    // but not
    //     a = b = c;  ==>  (a = (b = c, b), a);
    if (!BackstrokeUtility::isReturnValueUsed(e))
        return;

    // The following binary expressions return lvalue.
    if (SageInterface::isAssignmentStatement(e))
    {
        // a = b  ==>  a = b, a
        SgExpression* new_exp = buildBinaryExpression<SgCommaOpExp>(
                    copyExpression(e), 
                    copyExpression(isSgBinaryOp(e)->get_lhs_operand()));
        replaceExpression(e, new_exp);
        e = new_exp;
    }
    else if (isSgPlusPlusOp(e) || isSgMinusMinusOp(e))
    {
        // prefix ++ or -- returns lvalues, which we can hoist.
        // --a  ==>  --a, a
        if (isSgUnaryOp(e)->get_mode() == SgUnaryOp::prefix)
        {
            SgExpression* new_exp = buildBinaryExpression<SgCommaOpExp>(
                    copyExpression(e), 
                    copyExpression(isSgUnaryOp(e)->get_operand()));
            replaceExpression(e, new_exp);
            e = new_exp;
        }
        
        // Postfix ++ or -- returns rvalues, which is not the same value as after
        // this operation. We can delay them, but should take care when doing it.
        // a--  ==>  t = a, --a, t
        else
        {
            // A temporary variable is declared to hold the old value of the operand.
            // This declaration is put at the beginning of its closest basic block.

            // Get the closest basic block which contains the expression e.
            SgBasicBlock* body = getCurrentBody(e);
            SgName name = SageInterface::generateUniqueVariableName(body, "t");
            SgVariableDeclaration* temp_decl = buildVariableDeclaration(name, e->get_type(), NULL, getScope(e));
            body->prepend_statement(temp_decl);

            // The following code replaces
            //    a--
            // by
            //    t = a, --a, t
            SgInitializedName* init_name = temp_decl->get_decl_item(name);
            ROSE_ASSERT(init_name->get_scope());
            SgExpression* ass = buildBinaryExpression<SgAssignOp>(
                    buildVarRefExp(init_name, getScope(e)), 
                    copyExpression(isSgUnaryOp(e)->get_operand()));
            SgExpression* comma_exp = buildBinaryExpression<SgCommaOpExp>(
                    ass, copyExpression(e));
            comma_exp = buildBinaryExpression<SgCommaOpExp>(comma_exp, buildVarRefExp(init_name, getScope(e)));
            replaceExpression(e, comma_exp);
            e = comma_exp;
        }
    }
#if 0
    else if (isSgAndOp(e) || isSgOrOp(e))
    {
        // a && b  ==>  t = a, t && (t = b), t
        // a || b  ==>  t = a, t || (t = b), t
       
        SgBasicBlock* body = getCurrentBody(e);
        SgName name = BackstrokeUtility::GenerateUniqueVariableName(body, "t");
        SgVariableDeclaration* temp_decl = buildVariableDeclaration(name, buildBoolType(), NULL, getScope(e));
        body->prepend_statement(temp_decl);

        SgInitializedName* init_name = temp_decl->get_decl_item(name);
        SgVarRefExp* temp_var = buildVarRefExp(init_name, getScope(e));

        SgExpression* ass1 = buildBinaryExpression<SgAssignOp>(
                copyExpression(temp_var),
                copyExpression(isSgBinaryOp(e)->get_lhs_operand()));
        SgExpression* ass2 = buildBinaryExpression<SgAssignOp>(
                copyExpression(temp_var),
                copyExpression(isSgBinaryOp(e)->get_rhs_operand()));


        SgBinaryOp* new_exp = isSgBinaryOp(copyExpression(e));
        new_exp->set_lhs_operand(copyExpression(temp_var));
        new_exp->set_rhs_operand(ass2);

        SgExpression* comma_exp = buildBinaryExpression<SgCommaOpExp>(ass1, new_exp);
        comma_exp = buildBinaryExpression<SgCommaOpExp>(comma_exp, copyExpression(temp_var));
        replaceExpression(e, comma_exp);

        ass1 = propagateCommaOpExp(ass1);
        ass2 = propagateCommaOpExp(ass2);
    }
    else if (isSgConditionalExp(e))
    {
        // If the conditional expression returns rvalue, we can transform it into
        // a ? b : c  ==>  a ? (t = b) : (t = c), t
        // Else, if it returns and is used as a lvalue, we need more agressive transformation:
        // (a ? b : c) = d  ==>  a ? (b = d) : (c = d)
    }
#endif
}

SgExpression* propagateCommaOpExp(SgExpression* exp)
{
    // The normalized result.
    SgCommaOpExp* new_comma_op = NULL;

    if (SgBinaryOp* bin_op = isSgBinaryOp(exp))
    {
        // Ignore comma operator.
        if (isSgCommaOpExp(bin_op))
            return exp;

        SgExpression* lhs = bin_op->get_lhs_operand();
        SgExpression* rhs = bin_op->get_rhs_operand();

        // (a, b) + c  ==>  (a, b + c)
        if (SgCommaOpExp* comma_op = isSgCommaOpExp(lhs))
        {
            // If this binary operation is a dot expression, and it is used to reference
            // a member function, like (a, b).foo(c), we have to trace up to the function
            // call expression. The lhs operand of the dot exp above is (a, b) and rhs is
            // foo, but the whole expression is a function call. So we have to replace this
            // function call by the comma exp.
            if (SgDotExp* dot_exp = isSgDotExp(bin_op))
            {
                if (SgFunctionCallExp* func_call_exp =
                        isSgFunctionCallExp(dot_exp->get_parent()))
                {
                    SgDotExp* new_dot_exp = isSgDotExp(copyExpression(dot_exp));
                    new_dot_exp->set_lhs_operand(copyExpression(comma_op->get_rhs_operand()));
                    new_dot_exp->set_rhs_operand(copyExpression(dot_exp->get_rhs_operand()));

                    SgFunctionCallExp* new_func_call =
                            isSgFunctionCallExp(copyExpression(func_call_exp));
                    new_func_call->set_function(new_dot_exp);
                    new_func_call->set_args(
                        isSgExprListExp(copyExpression(func_call_exp->get_args())));

                    new_comma_op = buildBinaryExpression<SgCommaOpExp>(
                            copyExpression(comma_op->get_lhs_operand()), new_func_call);
                    replaceExpression(func_call_exp, new_comma_op);
                }
            }
            else
            {
                SgBinaryOp* new_exp = isSgBinaryOp(copyExpression(bin_op));
                new_exp->set_lhs_operand(copyExpression(comma_op->get_rhs_operand()));
                new_exp->set_rhs_operand(copyExpression(rhs));

                new_comma_op = buildBinaryExpression<SgCommaOpExp>(
                        copyExpression(comma_op->get_lhs_operand()), new_exp);
                replaceExpression(bin_op, new_comma_op);
            }
        }

        // Operator || and && cannot use the following transformation
        else if (!isSgAndOp(bin_op) && !isSgOrOp(bin_op))
        {
            // a + (b, c)  ==>  (b, a + c)
            if (SgCommaOpExp* comma_op = isSgCommaOpExp(rhs))
            {
                SgBinaryOp* new_exp = isSgBinaryOp(copyExpression(bin_op));
                new_exp->set_lhs_operand(copyExpression(lhs));
                new_exp->set_rhs_operand(copyExpression(comma_op->get_rhs_operand()));

                new_comma_op = buildBinaryExpression<SgCommaOpExp>(
                        copyExpression(comma_op->get_lhs_operand()), new_exp);
                replaceExpression(bin_op, new_comma_op);
            }
        }
    }

    else if (SgUnaryOp* unary_op = isSgUnaryOp(exp))
    {
        SgExpression* operand = unary_op->get_operand();

        // !(a, b)  ==>  (a, !b)
        if (SgCommaOpExp* comma_op = isSgCommaOpExp(operand))
        {
            SgUnaryOp* new_exp = isSgUnaryOp(copyExpression(unary_op));
            new_exp->set_operand(copyExpression(comma_op->get_rhs_operand()));

            new_comma_op = buildBinaryExpression<SgCommaOpExp>(
                    copyExpression(comma_op->get_lhs_operand()), new_exp);
            replaceExpression(unary_op, new_comma_op);
        }
    }

    else if (SgConditionalExp* cond_exp = isSgConditionalExp(exp))
    {   
        SgExpression* cond = cond_exp->get_conditional_exp();

        // (a, b) ? c : d  ==>  (a, b ? c : d)
        if (SgCommaOpExp* comma_op = isSgCommaOpExp(cond))
        {
            SgConditionalExp* new_cond_exp = isSgConditionalExp(copyExpression(cond_exp));
            new_cond_exp->set_conditional_exp(copyExpression(comma_op->get_rhs_operand()));
            new_comma_op = buildBinaryExpression<SgCommaOpExp>(
                    copyExpression(comma_op->get_lhs_operand()), new_cond_exp);
            replaceExpression(cond_exp, new_comma_op);
        }
    }

    if (new_comma_op)
    {
        propagateCommaOpAndConditionalExp(new_comma_op->get_lhs_operand());
        propagateCommaOpAndConditionalExp(new_comma_op->get_rhs_operand());
        return new_comma_op;
    }
    return exp;
}

// After the following transformation, a conditional expression should not be the operand
// of another expression except comma and conditional expressions. This will make its forward
// and reverse code generation easier.
SgExpression* propagateConditionalExp(SgExpression* exp)
{
    if (SgBinaryOp* bin_op = isSgBinaryOp(exp))
    {
        // Ignore comma operator.
        if (isSgCommaOpExp(bin_op))
            return exp;

        SgExpression* lhs = bin_op->get_lhs_operand();
        SgExpression* rhs = bin_op->get_rhs_operand();

        // (a ? b : c) + d  ==>  a ? (b + d) : (c + d)
        if (SgConditionalExp* cond_op = isSgConditionalExp(lhs))
        {
            SgExpression* true_exp = cond_op->get_true_exp();
            SgExpression* false_exp = cond_op->get_false_exp();

            SgBinaryOp* new_true_exp = isSgBinaryOp(copyExpression(bin_op));
            new_true_exp->set_lhs_operand(copyExpression(true_exp));
            new_true_exp->set_rhs_operand(copyExpression(rhs));
            replaceExpression(true_exp, new_true_exp);

            SgBinaryOp* new_false_exp = isSgBinaryOp(copyExpression(bin_op));
            new_false_exp->set_lhs_operand(copyExpression(false_exp));
            new_false_exp->set_rhs_operand(copyExpression(rhs));
            replaceExpression(false_exp, new_false_exp);

            // Copy the old cond_op since replacement will destroy it.
            SgConditionalExp* new_cond_op = isSgConditionalExp(copyExpression(cond_op));
            replaceExpression(bin_op, new_cond_op);

            propagateCommaOpAndConditionalExp(new_cond_op->get_true_exp());
            propagateCommaOpAndConditionalExp(new_cond_op->get_false_exp());

            return new_cond_op;
        }

        // Operator || and && cannot use the following transformation
        if (!isSgAndOp(bin_op) && !isSgOrOp(bin_op))
        {
            // a + (b ? c : d)  ==>  b ? (a + c) : (a + d)
            if (SgConditionalExp* cond_op = isSgConditionalExp(rhs))
            {
                SgExpression* true_exp = cond_op->get_true_exp();
                SgExpression* false_exp = cond_op->get_false_exp();

                SgBinaryOp* new_true_exp = isSgBinaryOp(copyExpression(bin_op));
                new_true_exp->set_lhs_operand(copyExpression(lhs));
                new_true_exp->set_rhs_operand(copyExpression(true_exp));
                replaceExpression(true_exp, new_true_exp);

                SgBinaryOp* new_false_exp = isSgBinaryOp(copyExpression(bin_op));
                new_false_exp->set_lhs_operand(copyExpression(lhs));
                new_false_exp->set_rhs_operand(copyExpression(false_exp));
                replaceExpression(false_exp, new_false_exp);

                // Copy the old cond_op since replacement will destroy it.
                SgConditionalExp* new_cond_op = isSgConditionalExp(copyExpression(cond_op));
                replaceExpression(bin_op, new_cond_op);

                propagateCommaOpAndConditionalExp(new_cond_op->get_true_exp());
                propagateCommaOpAndConditionalExp(new_cond_op->get_false_exp());

                return new_cond_op;
            }
        }
    }

    if (SgUnaryOp* unary_op = isSgUnaryOp(exp))
    {
        SgExpression* operand = unary_op->get_operand();

        // !(a ? b : c)  ==>  a ? !b : !c
        if (SgConditionalExp* cond_op = isSgConditionalExp(operand))
        {
            SgExpression* true_exp = cond_op->get_true_exp();
            SgExpression* false_exp = cond_op->get_false_exp();

            SgUnaryOp* new_true_exp = isSgUnaryOp(copyExpression(unary_op));
            new_true_exp->set_operand(copyExpression(true_exp));
            replaceExpression(true_exp, new_true_exp);

            SgUnaryOp* new_false_exp = isSgUnaryOp(copyExpression(unary_op));
            new_false_exp->set_operand(copyExpression(false_exp)); 
            replaceExpression(false_exp, new_false_exp);

            // Copy the old cond_op since replacement will destroy it.
            SgConditionalExp* new_cond_op = isSgConditionalExp(copyExpression(cond_op));
            replaceExpression(unary_op, new_cond_op);

            propagateCommaOpAndConditionalExp(new_cond_op->get_true_exp());
            propagateCommaOpAndConditionalExp(new_cond_op->get_false_exp());

            return new_cond_op;
        }
    }

    // Condtional exp as condition of another conditional exp.
    // (a ? b : c) ? d : e  ==>  a ? (b ? d : e) : (c ? d : e)
    if (SgConditionalExp* cond_exp = isSgConditionalExp(exp))
    {   
        SgExpression* cond = cond_exp->get_conditional_exp();

        if (SgConditionalExp* cond_op = isSgConditionalExp(cond))
        {
            SgConditionalExp* cond_true_exp = isSgConditionalExp(copyExpression(cond_exp));
            SgConditionalExp* cond_false_exp = isSgConditionalExp(copyExpression(cond_exp));

            cond_true_exp->set_conditional_exp(copyExpression(cond_op->get_true_exp()));
            cond_false_exp->set_conditional_exp(copyExpression(cond_op->get_false_exp()));

            SgConditionalExp* new_cond_op = buildConditionalExp(
                    copyExpression(cond_op->get_conditional_exp()), 
                    cond_true_exp, 
                    cond_false_exp);
            replaceExpression(cond_exp, new_cond_op);

            propagateCommaOpAndConditionalExp(new_cond_op->get_true_exp());
            propagateCommaOpAndConditionalExp(new_cond_op->get_false_exp());

            return new_cond_op;
        }
    }

    return exp;
}


/** Move all declarations in condition/test/selector part in if/while/for/switch statements out to
    a new basic block which also contains that if/while/for/switch statement. */
void moveDeclarationsOut(SgNode* node)
{
    // Before dealing with variable declarations, all for loops should be processed first.
    // This is because that SgForInitStatement is special in which several declarations can
    // coexist. We will hoist it outside of its for loop statement.
    
    vector<SgForInitStatement*> for_init_stmts = BackstrokeUtility::querySubTree<SgForInitStatement >(node);
    foreach (SgForInitStatement* for_init_stmt, for_init_stmts)
    {
        // A SgForInitStatement object can contain several variable declarations, or one expression statement.
        SgStatementPtrList stmts = for_init_stmt->get_init_stmt();
        if (!stmts.empty() && isSgVariableDeclaration(stmts[0]))
        {
            SgForStatement* for_stmt = isSgForStatement(for_init_stmt->get_parent());
            SgBasicBlock* new_block = buildBasicBlock();
            foreach (SgStatement* decl, stmts)
            {
                SgVariableDeclaration* var_decl = isSgVariableDeclaration(decl);
                ROSE_ASSERT(var_decl);
                //appendStatement(copyStatement(var_decl), new_block);
                appendStatement(var_decl, new_block);
                //delete var_decl;
            }
            
            // Since there is no builder function for SgForInitStatement, we build it by ourselves
            SgForInitStatement* null_for_init = buildForInitStatement(SgStatementPtrList());
            replaceStatement(for_init_stmt, null_for_init);

            // It seems that 'for_init_stmt' should be deleted explicitly.
            //deepDelete(for_init_stmt);

            appendStatement(copyStatement(for_stmt), new_block);
            replaceStatement(for_stmt, new_block);
            //deepDelete(for_stmt);
        }
    }

#if 0
    // Separate variable's definition from its declaration
    // FIXME It is not sure that whether to permit declaration in condition of if (if the varible declared
    // is not of scalar type?).
    vector<SgVariableDeclaration*> var_decl_list = BackstrokeUtility::querySubTree<SgVariableDeclaration>(node);
    foreach (SgVariableDeclaration* var_decl, var_decl_list)
    {
        SgInitializedName* init = var_decl->get_variables()[0];
        //SgAssignInitializer* initializer = isSgAssignInitializer(init->get_initializer());

        SgStatement* parent = isSgStatement(var_decl->get_parent());
        ROSE_ASSERT(parent);

        //if (isScalarType(init->get_type()) || isSgPointerType(init->get_type()))
        {
            // Miss catch here?
            if (isSgIfStmt(parent) ||
                    isSgWhileStmt(parent) ||
                    isSgSwitchStatement(parent) ||
                    isSgForStatement(parent))
            {
                // if (int i = j);  ==>  { int i = j; if (i); }
                // new_exp  <==  j
                SgExpression* new_exp = buildVarRefExp(init, getScope(var_decl));

                SgBasicBlock* block = buildBasicBlock();

                // Note that we cannot just copy the declaration because doing so will not
                // build a new symbol for the variable declared.
                //SgVariableDeclaration* new_decl = isSgVariableDeclaration(copyStatement(var_decl));

                // To build the correct symbols, we build a new variable declaration.
                // new_decl  <==  int i = j;
                SgVariableDeclaration* new_decl = buildVariableDeclaration(
                        init->get_name(),
                        init->get_type(),
                        init->get_initializer(),
                        block);
                // Remember to move the preprocessing information from the old declaration to the new one.
                movePreprocessingInfo(var_decl, new_decl);

                replaceStatement(var_decl, buildExprStatement(new_exp));
                //SgBasicBlock* block = buildBasicBlock(copyStatement(parent));
                appendStatement(new_decl, block);
                appendStatement(copyStatement(parent), block);

                replaceStatement(parent, block);
            }
        }
    }
#endif
}

void preprocess(SgNode* node)
{
    /******************************************************************************/
    // To ensure every if, while, etc. has a basic block as its body.
    

    changeAllBodiesToBlocks (getProject());
#if 0    
    vector<SgStatement*> stmt_list = BackstrokeUtility::querySubTree<SgStatement>(node);
    foreach (SgStatement* stmt, stmt_list)
    {
        // Liao 3/2/2012, avoid using this function since its messy semantics
        //ensureBasicBlockAsParent(stmt);

        // If the if statement does not have a else body, we will build one for it.
        if (SgIfStmt* if_stmt = isSgIfStmt(stmt))
        {
            if (if_stmt->get_false_body() == NULL)
            {
                SgBasicBlock* empty_block = buildBasicBlock();
                if_stmt->set_false_body(empty_block);
                empty_block->set_parent(if_stmt);
            }
        }
    }
#endif
    /******************************************************************************/
    // Move all declarations in condition/test/selector part in if/while/for/switch 
    // statements out to a new basic block which also contains that if/while/for/switch statement.

    moveDeclarationsOut(node);


    /******************************************************************************/
    // Transform logical and & or operators into conditional expression if the rhs operand contains defs.
    // a && b  ==>  a ? b : false
    // a || b  ==>  a ? true : b

    vector<SgAndOp*> and_exps = BackstrokeUtility::querySubTree<SgAndOp>(node);
    foreach (SgAndOp* and_op, and_exps)
    {
        if (containsModifyingExpression(and_op->get_rhs_operand()))
        {
            SgConditionalExp* cond = buildConditionalExp(
                    copyExpression(and_op->get_lhs_operand()),
                    copyExpression(and_op->get_rhs_operand()),
                    buildBoolValExp(true));
            replaceExpression(and_op, cond);
        }
    }

    vector<SgOrOp*> or_exps = BackstrokeUtility::querySubTree<SgOrOp>(node);
    foreach (SgOrOp* or_op, or_exps)
    {
        if (containsModifyingExpression(or_op->get_rhs_operand()))
        {
            SgConditionalExp* cond = buildConditionalExp(
                    copyExpression(or_op->get_lhs_operand()),
                    buildBoolValExp(true),
                    copyExpression(or_op->get_rhs_operand()));
            replaceExpression(or_op, cond);
        }
    }
}

bool isInSizeOfOp(SgNode* node)
{
	while ((node = node->get_parent()))
	{
		if (isSgSizeOfOp(node))
			return true;
	}
	return false;
}

/** Split a comma expression into several statements. */
void turnCommaOpExpIntoStmt(SgExpression* exp)
{
	SgCommaOpExp* comma_op = isSgCommaOpExp(exp);
	if (!comma_op) return;

	SgExpression* lhs = comma_op->get_lhs_operand();
	SgExpression* rhs = comma_op->get_rhs_operand();

	//lhs->set_need_paren(false);
	//rhs->set_need_paren(false);

	SgNode* parent = comma_op->get_parent();

	if (SgStatement* stmt = isSgExprStatement(parent))
	{
		switch (stmt->get_parent()->variantT())
		{
			case V_SgBasicBlock:
			{
				// Note that in Rose, the condition part in if statement can be an expression statement.
				// (a, b);  ==>  a; b;
				
				SgExprStatement* lhs_stmt = buildExprStatement(copyExpression(lhs));
				SgExprStatement* rhs_stmt = buildExprStatement(copyExpression(rhs));
				SgStatement* new_stmt = buildBasicBlock(lhs_stmt, rhs_stmt);
				replaceStatement(stmt, new_stmt, true);
				
				normalizeExpressions(new_stmt);
				break;
			}

			case V_SgIfStmt:
			case V_SgSwitchStatement:
			{
				// Split comma expression in if, while, switch condition or selection part.
				// For example, if (a, b);  ==>  a; if (b);
				
				SgExprStatement* new_stmt = buildExprStatement(copyExpression(lhs));
				SgExpression* new_exp = copyExpression(rhs);
				replaceExpression(comma_op, new_exp);
				insertStatement(isSgStatement(stmt->get_parent()), new_stmt);

				normalizeExpressions(new_stmt);
				normalizeExpressions(new_exp);
				break;
			}

			case V_SgWhileStmt:
			{
#if 0
				SgExprStatement* new_stmt = buildExprStatement(copyExpression(lhs));
				SgExpression* new_exp = copyExpression(rhs);
				replaceExpression(comma_op, new_exp);
				insertStatement(isSgStatement(stmt->get_parent()), new_stmt);

				normalizeExpressions(new_stmt);
				normalizeExpressions(new_exp);

				// FIXME!: Now it's not clear how to deal with continue in while. If it's transformed into
				// goto, then we don't have to put the side effect in condition before continue.
#endif
				break;
			}
			
			case V_SgForStatement:
			case V_SgDoWhileStmt:
			{
				// FIXME  while?? do-while?? for???
				//ROSE_ASSERT(false);
				break;
			}

			default:
				break;
		}
	}

	// Split comma expression across declarations.
	// For example, int a = (b, c);  ==>  b; int a = c;
	else if (SgAssignInitializer* ass_init = isSgAssignInitializer(parent))
	{
		if (SgVariableDeclaration* var_decl = isSgVariableDeclaration(ass_init->get_parent()->get_parent()))
		{
			// This is very important!
			ROSE_ASSERT(var_decl->get_variables().size() == 1);

			// Note that a variable declaration can appear in the condition part of if, for, etc.
			// Here we only deal with those which are exactly in a basic block.
			// After preprocessing, all declarations inside of if/for/etc. will be extracted out.
			// int i = (a, b);  ==>  a; int i = b;
			if (isSgBasicBlock(var_decl->get_parent()))
			{
				SgExprStatement* new_stmt = buildExprStatement(copyExpression(lhs));
				SgExpression* new_exp = copyExpression(rhs);
				replaceExpression(comma_op, new_exp);
				insertStatementBefore(var_decl, new_stmt);

				normalizeExpressions(new_stmt);
				normalizeExpressions(new_exp);
			}
		}
		//FIXME other cases
		else
		{
			cout << ass_init->get_parent()->get_parent()->class_name() << endl;
			//ROSE_ASSERT(false);
		}
	}

	else if (SgReturnStmt* return_stmt = isSgReturnStmt(parent))
	{
		SgExprStatement* lhs_stmt = buildExprStatement(copyExpression(lhs));
		SgReturnStmt* rhs_stmt = buildReturnStmt(copyExpression(rhs));
		SgStatement* new_stmt = buildBasicBlock(lhs_stmt, rhs_stmt);
		replaceStatement(return_stmt, new_stmt, true);
		
		normalizeExpressions(new_stmt);
	}

	else
	{
		//cout << parent->class_name() << ':' << get_name(parent) << endl;
		//ROSE_ASSERT(false);
	}
}


void turnConditionalExpIntoStmt(SgExpression* exp)
{
	SgConditionalExp* conditional_exp = isSgConditionalExp(exp);
	if (!conditional_exp) return;

	SgExpression* cond = conditional_exp->get_conditional_exp();
	SgExpression* true_exp = conditional_exp->get_true_exp();
	SgExpression* false_exp = conditional_exp->get_false_exp();

	SgNode* parent = conditional_exp->get_parent();
	
	if (SgExprStatement* stmt = isSgExprStatement(parent))
	{
		switch (stmt->get_parent()->variantT())
		{
			case V_SgBasicBlock:
			{
				// a ? b : c  ==>  if (a) b; else c;
				
				SgStatement* cond_stmt = buildExprStatement(copyExpression(cond));
				SgStatement* true_stmt = buildBasicBlock(buildExprStatement(copyExpression(true_exp)));
				SgStatement* false_stmt = buildBasicBlock(buildExprStatement(copyExpression(false_exp)));
				SgStatement* new_stmt = buildIfStmt(cond_stmt, true_stmt, false_stmt);
				
				replaceStatement(stmt, new_stmt, true);
				normalizeExpressions(new_stmt);
				break;
			}

			default:
				break;
		}
	}
	
	else if (SgReturnStmt* return_stmt = isSgReturnStmt(parent))
	{
		SgStatement* cond_stmt = buildExprStatement(copyExpression(cond));
		SgStatement* true_stmt = buildBasicBlock(buildReturnStmt(copyExpression(true_exp)));
		SgStatement* false_stmt = buildBasicBlock(buildReturnStmt(copyExpression(false_exp)));
		SgStatement* new_stmt = buildIfStmt(cond_stmt, true_stmt, false_stmt);
		
		replaceStatement(return_stmt, new_stmt, true);
		normalizeExpressions(new_stmt);
	}

	// Split conditional expression across declarations.
	// For example, int i = a ? b : c;  ==>  int i; a ? i = b : i = c;
	// Note now we can only perform this transformation on scalar types!
	else if (SgAssignInitializer* ass_init = isSgAssignInitializer(parent))
	{
		if (SgVariableDeclaration* var_decl = isSgVariableDeclaration(ass_init->get_parent()->get_parent()))
		{
			// This is very important!
			ROSE_ASSERT(var_decl->get_variables().size() == 1);

			SgType* type = var_decl->get_variables().front()->get_type();
            //cout << type->class_name() << endl;
            type = type->stripTypedefsAndModifiers();
			ROSE_ASSERT(isScalarType(type) || isSgEnumType(type));
			
			// Note that a variable declaration can appear in the condition part of if, for, etc.
			// Here we only deal with those which are exactly in a basic block.
			// After preprocessing, all declarations inside of if/for/etc. will be extracted out.
			// int i = a ? b : c;  ==>  int i; i = a ? b : c;
			
			if (isSgBasicBlock(var_decl->get_parent()))
			{
				SgExpression* new_exp =	buildBinaryExpression<SgAssignOp>(
						buildVarRefExp(var_decl->get_variables().front()),
						copyExpression(conditional_exp));
				SgExprStatement* new_stmt = buildExprStatement(new_exp);

				// Remove the previous initializer
				deepDelete(var_decl->get_variables().front()->get_initializer());
				var_decl->get_variables().front()->set_initializer(NULL);

				insertStatementAfter(var_decl, new_stmt);
				normalizeExpressions(new_stmt);
			}
		}
		
		//FIXME other cases
		else
			ROSE_ASSERT(false);
	}

	else
	{
		cout << parent->class_name() << ':' << get_name(parent) << endl;
		//ROSE_ASSERT(false);
	}
}

} // namespace BackstrokeNormUtility

} // namespace BackstrokeNorm

