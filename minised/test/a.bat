@REM ����ļ�������dos�����У���unix����лᱨ��
@REM cl /EHsc /showIncludes main.cpp > main.inc

@REM �޳����ļ���ʣ�µ�д��one.txt�󲢴ӱ�׼�����ɾ������bug��foo.h��one.txt�г�������
TYPE main.inc | ..\minised -e "/Note: including file:[ \t]\+[A-Z]:\\Program Files/d" -e "/Note: including file:/w bug.txt" -e "/Note: including file:/d"

@REM �����һ��-e�������ùܵ��ٴθ�minised���Ϳ��Ա������bug
TYPE main.inc | ..\minised -e "/Note: including file:[ \t]\+[A-Z]:\\Program Files/d" -e "/Note: including file:/w ok.txt" | ..\minised -e "/Note: including file:/d"

@REM ��������a.sh�Ը�����sed����Ϊ���Ա�


