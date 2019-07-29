#include <QApplication>
#include <QDialog>

#include "ui_gotocelldialog.h"

// vc-extra-compile-flags: -IF:\vcpkg\installed\x86-windows\include\QtWidgets

/* cpps-make
ui_gotocelldialog.h : gotocelldialog.ui
	uic gotocelldialog.ui -o gotocelldialog.h
	del ui_gotocelldialog.h > NUL 2>&1
	ren gotocelldialog.h ui_gotocelldialog.h
*/
// �������Ƕ���make����������Ӹ�������Ҫ��ô���ӣ��õ���make���ɡ���ֻ��Ϊ�˾ٸ����ӡ�//֮���Ǹ�-��Ϊ�˱���cpps���䵱��ָ��
//- cpps-make ui_gotocelldialog.h : gotocelldialog.ui // uic gotocelldialog.ui -o ui_gotocelldialog.h

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    Ui::GoToCellDialog ui;
    QDialog *dialog = new QDialog;
    ui.setupUi(dialog);
    dialog->show();

    return app.exec();
}
