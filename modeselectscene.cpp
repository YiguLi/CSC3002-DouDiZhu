#include "modeselectscene.h"
#include "ui_modeselectscene.h"
#include "ingamescene.h"
#include "lobbyscene.h"

ModeSelectScene::ModeSelectScene(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ModeSelectScene)
{
    ui->setupUi(this);
    this->setWindowTitle("选择游戏模式");
    this->setFixedSize(800, 600);
}

ModeSelectScene::~ModeSelectScene()
{
    delete ui;
}

void ModeSelectScene::on_singlePlayerBtn_clicked()
{
    // 进入单机游戏
    InGameScene *w = new InGameScene();
    w->show();
    this->hide();
}

void ModeSelectScene::on_multiPlayerBtn_clicked()
{
    // 进入联机大厅
    LobbyScene *w = new LobbyScene();
    w->show();
    this->hide();
}

void ModeSelectScene::on_backBtn_clicked()
{
    this->close();
}
