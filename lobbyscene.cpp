#include "lobbyscene.h"
#include "ui_lobbyscene.h"
#include "roomscene.h"
#include <QInputDialog>
#include <QMessageBox>

LobbyScene::LobbyScene(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LobbyScene)
{
    ui->setupUi(this);
    this->setWindowTitle("联机大厅");
    this->setFixedSize(800, 600);
}

LobbyScene::~LobbyScene()
{
    delete ui;
}

void LobbyScene::on_createRoomBtn_clicked()
{
    // 创建房间 - 作为服务器
    RoomScene *w = new RoomScene(true, this); // true = 是服务器
    w->show();
    this->hide();
}

void LobbyScene::on_joinRoomBtn_clicked()
{
    // 加入房间 - 作为客户端，需要输入IP
    bool ok;
    QString ip = QInputDialog::getText(this, 
                                       "加入房间",
                                       "请输入房主的IP地址:",
                                       QLineEdit::Normal,
                                       "192.168.", 
                                       &ok);
    
    if (ok && !ip.isEmpty()) {
        RoomScene *w = new RoomScene(false, this); // false = 是客户端
        w->connectToServer(ip);
        w->show();
        this->hide();
    }
}

void LobbyScene::on_backBtn_clicked()
{
    this->close();
}
