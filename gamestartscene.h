#ifndef GAMESTARTSCENE_H
#define GAMESTARTSCENE_H

#include <QDialog>

namespace Ui {
class Gamestartscene;
}

class Gamestartscene : public QDialog
{
    Q_OBJECT

public:
    explicit Gamestartscene(QWidget *parent = nullptr);
    ~Gamestartscene();

private slots:
    void on_start_clicked();
    void on_online_clicked();
    void on_exit_clicked();

private:
    Ui::Gamestartscene *ui;
};

#endif // GAMESTARTSCENE_H
