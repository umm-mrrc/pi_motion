#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "stepper.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    
private slots:
    void on_step_execute_clicked();
    void on_step_stop_clicked();
    void on_step1_clearAll_clicked();
    void on_step1_clearSelected_clicked();
    void on_step1_motionQueue_currentRowChanged(int currentRow);
    void on_step1_queueAfter_clicked();
    void on_step1_loopBefore_clicked();
    void on_step1_loopAfter_clicked();
    void on_step1_queueBefore_clicked();

    void on_step1_moveUp_clicked();

    void on_step1_moveDown_clicked();

    void on_step2_queueAfter_clicked();

    void on_step2_queueBefore_clicked();

    void on_step2_loopAfter_clicked();

    void on_step2_loopBefore_clicked();

    void on_step2_clearSelected_clicked();

    void on_step2_clearAll_clicked();

    void on_step2_moveUp_clicked();

    void on_step2_moveDown_clicked();

private:
    Ui::MainWindow *ui;
    stepper stepperObj;
    int stepperLoops[NUM_MOTORS];
};

#endif // MAINWINDOW_H
