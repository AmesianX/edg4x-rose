/*
 * This code tests a number of features of the implicit call generator.
 * It contains a tricky edge case involving nested constructors.
 */
#define CLS(NAME) \
struct NAME \
   { \
     bool b; \
     NAME() : b(false) {} \
     NAME(bool b) : b(b) {} \
     ~NAME() {} \
     operator bool() { return true; } \
   }

CLS(A);
CLS(B);
CLS(C);
CLS(D);
CLS(E);

int
main()
   {
     int a;
     if ( A() );

     if ((A() && B(C() && D())) || E())
        {
        }

     return 0;
   }
