@ECHO OFF	

cpps --clear show-args.cpp a b "c d"
rem CHECK: argc: 4
rem CHECK-NEXT: arg[1]: a
rem CHECK-NEXT: arg[2]: b
rem CHECK-NEXT: arg[3]: c d
