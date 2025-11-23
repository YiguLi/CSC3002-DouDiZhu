#include "gamestartscene.h"
#include "ui_gamestartscene.h"
#include "modeselectscene.h"

Gamestartscene::Gamestartscene(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Gamestartscene)
{
    ui->setupUi(this);
    this -> setWindowTitle("start");
    this->setFixedSize(800, 600);
}

Gamestartscene::~Gamestartscene()
{
    delete ui;
}
//进入模式选择界面
void Gamestartscene::on_start_clicked()
{
    ModeSelectScene *w = new ModeSelectScene();
    w -> show();
    this -> hide();
}


//退出程序
void Gamestartscene::on_exit_clicked()
{
    this -> close();
}

