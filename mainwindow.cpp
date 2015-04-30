#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>
#include "stepper.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    for (int i = 0; i < NUM_MOTORS; i++) {
        stepperLoops[i] = 0;
    }
    std::cout << "Done setup\n";
}

MainWindow::~MainWindow()
{
    std::cout << "Close up shop!\n";
    delete ui;
}

void MainWindow::on_step_execute_clicked()
{
    QList<int> loopStartIndex;
    // Clear all currently queued commands...
    stepperObj.clearAll();
    // (Re)queue all the commands in the stepper1 list...
    const char *currCmd;
    for (int i = 0; i < ui->step1_motionQueue->count(); i++) {
        // Queue a "move" command...
        if (ui->step1_motionQueue->item(i)->text().startsWith("move")) {
            currCmd = qPrintable(ui->step1_motionQueue->item(i)->text());
            double distance, duration;
            char str[100];
            sscanf(currCmd, "%s %lf %lf", str, &distance, &duration);
            std::cout << "Moving " << distance << " over " << duration << "\n";
            stepperObj.queueMoveCmd(0, distance, duration, 1.0);
        }
        else if (ui->step1_motionQueue->item(i)->text().startsWith("pause")) {
            currCmd = qPrintable(ui->step1_motionQueue->item(i)->text());
            double duration;
            char str[100];
            sscanf(currCmd, "%s %lf", str, &duration);
            std::cout << "Pausing for " << duration << "\n";
            stepperObj.queuePauseCmd(0, duration);
        }
        // Queue a "loop" command...
        else if (ui->step1_motionQueue->item(i)->text().startsWith("loop")) {
            currCmd = qPrintable(ui->step1_motionQueue->item(i)->text());
            int loopNum, numIters;
            char str[100];
            sscanf(currCmd, "%s %d %d", str, &loopNum, &numIters);
            std::cout << "Loop start command = " << i << "\n";
            loopStartIndex << i;
            stepperObj.queueLoopStartCmd(0);
        }
        // Queue an "endloop" command...
        else if (ui->step1_motionQueue->item(i)->text().startsWith("endloop")) {
            currCmd = qPrintable(ui->step1_motionQueue->item(i)->text());
            int loopNum, numIters;
            char str[100];
            sscanf(currCmd, "%s %d %d", str, &loopNum, &numIters);
            int startCmd = loopStartIndex.takeLast();
            std::cout << "Loopend starting cmd = " << startCmd << "\n";
            stepperObj.queueLoopEndCmd(0, startCmd, numIters);
        }
    }

    // (Re)queue all the commands in the stepper2 list...
    for (int i = 0; i < ui->step2_motionQueue->count(); i++) {
        // Queue a "move" command...
        if (ui->step2_motionQueue->item(i)->text().startsWith("move")) {
            currCmd = qPrintable(ui->step2_motionQueue->item(i)->text());
            double distance, duration;
            char str[100];
            sscanf(currCmd, "%s %lf %lf", str, &distance, &duration);
            std::cout << "Moving " << distance << " over " << duration << "\n";
            stepperObj.queueMoveCmd(1, distance, duration, 1.0);
        }
        else if (ui->step2_motionQueue->item(i)->text().startsWith("pause")) {
            currCmd = qPrintable(ui->step2_motionQueue->item(i)->text());
            double duration;
            char str[100];
            sscanf(currCmd, "%s %lf", str, &duration);
            std::cout << "Pausing for " << duration << "\n";
            stepperObj.queuePauseCmd(1, duration);
        }
        // Queue a "loop" command...
        else if (ui->step2_motionQueue->item(i)->text().startsWith("loop")) {
            currCmd = qPrintable(ui->step2_motionQueue->item(i)->text());
            int loopNum, numIters;
            char str[100];
            sscanf(currCmd, "%s %d %d", str, &loopNum, &numIters);
            std::cout << "Loop start command = " << i << "\n";
            loopStartIndex << i;
            stepperObj.queueLoopStartCmd(1);
        }
        // Queue an "endloop" command...
        else if (ui->step2_motionQueue->item(i)->text().startsWith("endloop")) {
            currCmd = qPrintable(ui->step2_motionQueue->item(i)->text());
            int loopNum, numIters;
            char str[100];
            sscanf(currCmd, "%s %d %d", str, &loopNum, &numIters);
            int startCmd = loopStartIndex.takeLast();
            std::cout << "Loopend starting cmd = " << startCmd << "\n";
            stepperObj.queueLoopEndCmd(1, startCmd, numIters);
        }
    }
    // And start...
    stepperObj.startAll();
}

void MainWindow::on_step_stop_clicked()
{
    std::cout << "Stop everything!\n";
    stepperObj.resetAll();
}

void MainWindow::on_step1_clearAll_clicked()
{
    ui->step1_motionQueue->clear();
    stepperLoops[0] = 0;
}

void MainWindow::on_step1_clearSelected_clicked()
{
    int checkRow, currRow = ui->step1_motionQueue->currentRow();
    if (currRow < 0)
        return;
    int nItems;
    nItems = ui->step1_motionQueue->count();
    QListWidgetItem *currItem = ui->step1_motionQueue->item(currRow);
    QString data = currItem->text();
    int ldepth = 0;
    QListWidgetItem *delWI;
    // Delete a loop start - search forward for the matching loop end to delete...
    if (data.startsWith("loop")) {
        for (checkRow = currRow+1; checkRow < nItems; checkRow++) {
            if (ui->step1_motionQueue->item(checkRow)->text().startsWith("endloop")) {
                if (ldepth-- == 0) break;
            }
            else if (ui->step1_motionQueue->item(checkRow)->text().startsWith("loop")) {
                ldepth++;
            }
        }
        delWI = ui->step1_motionQueue->takeItem(checkRow);
        delete delWI;
        delWI = ui->step1_motionQueue->takeItem(currRow);
        delete delWI;
    }
    // Delete a loop end - search backward for the matching loop start to delete...
    else if (data.startsWith("endloop")) {
        for (checkRow = currRow-1; checkRow >= 0; checkRow--) {
            if (ui->step1_motionQueue->item(checkRow)->text().startsWith("loop")) {
                if (ldepth-- == 0) break;
            }
            else if (ui->step1_motionQueue->item(checkRow)->text().startsWith("endloop")) {
                ldepth++;
            }
        }
        delWI = ui->step1_motionQueue->takeItem(currRow);
        delete delWI;
        delWI = ui->step1_motionQueue->takeItem(checkRow);
        delete delWI;
    }
    // Delete a move command - just delete it...
    else {
        QListWidgetItem *delWI = ui->step1_motionQueue->takeItem(currRow);
        delete delWI;
    }
}

void MainWindow::on_step1_motionQueue_currentRowChanged(int currentRow)
{
//    std::cout << "Changed to " << currentRow << "\n";
}

void MainWindow::on_step1_queueBefore_clicked()
{
    double distance = ui->step1_distance->value();
    double duration = ui->step1_duration->value();
    char mstring[100];
    if (distance == 0) {
        sprintf(mstring, "pause %6.1f", duration);
    }
    else {
        sprintf(mstring, "move %6.1f %6.1f", distance, duration);
    }
    QListWidgetItem *stepItem = new QListWidgetItem;
    stepItem->setText(mstring);
    int currRow = ui->step1_motionQueue->currentRow();
    if (currRow < 0) {
        currRow = 0;
        ui->step1_motionQueue->insertItem(currRow, stepItem);
        ui->step1_motionQueue->setCurrentRow(currRow);
    }
    else {
        ui->step1_motionQueue->insertItem(currRow, stepItem);
        ui->step1_motionQueue->setCurrentRow(currRow + 1);
    }
}

void MainWindow::on_step1_queueAfter_clicked()
{
    double distance = ui->step1_distance->value();
    double duration = ui->step1_duration->value();
    char mstring[100];
    if (distance == 0) {
        sprintf(mstring, "pause %6.1f", duration);
    }
    else {
        sprintf(mstring, "move %6.1f %6.1f", distance, duration);
    }
    QListWidgetItem *stepItem = new QListWidgetItem;
    stepItem->setText(mstring);
    int currRow = ui->step1_motionQueue->currentRow() + 1;
    ui->step1_motionQueue->insertItem(currRow, stepItem);
    ui->step1_motionQueue->setCurrentRow(currRow);
}

void MainWindow::on_step1_loopBefore_clicked()
{
    int loopNum = ++stepperLoops[0];
    int loopCount = ui->step1_loopCount->value();
    char mstring[100];
    sprintf(mstring, "loop %d %d", loopNum, loopCount);
    QListWidgetItem *loopItem_start = new QListWidgetItem;
    loopItem_start->setText(mstring);
    sprintf(mstring, "endloop %d %d", loopNum, loopCount);
    QListWidgetItem *loopItem_end = new QListWidgetItem;
    loopItem_end->setText(mstring);
    int currRow = ui->step1_motionQueue->currentRow();
    if (currRow < 0) {
        currRow = 0;
    }
    ui->step1_motionQueue->insertItem(currRow, loopItem_start);
    ui->step1_motionQueue->insertItem(currRow+1, loopItem_end);
    ui->step1_motionQueue->setCurrentRow(currRow);
}

void MainWindow::on_step1_loopAfter_clicked()
{
    int loopNum = ++stepperLoops[0];
    int loopCount = ui->step1_loopCount->value();
    char mstring[100];
    sprintf(mstring, "loop %d %d", loopNum, loopCount);
    QListWidgetItem *loopItem_start = new QListWidgetItem;
    loopItem_start->setText(mstring);
    sprintf(mstring, "endloop %d %d", loopNum, loopCount);
    QListWidgetItem *loopItem_end = new QListWidgetItem;
    loopItem_end->setText(mstring);
    int currRow = ui->step1_motionQueue->currentRow() + 1;
    ui->step1_motionQueue->insertItem(currRow, loopItem_start);
    ui->step1_motionQueue->insertItem(currRow+1, loopItem_end);
    ui->step1_motionQueue->setCurrentRow(currRow);
}


void MainWindow::on_step1_moveUp_clicked()
{
    int currRow = ui->step1_motionQueue->currentRow();
    if (currRow < 0)
        return;
}

void MainWindow::on_step1_moveDown_clicked()
{
    int currRow = ui->step1_motionQueue->currentRow();
    if (currRow < 0)
        return;
}

void MainWindow::on_step2_queueAfter_clicked()
{
    double distance = ui->step2_distance->value();
    double duration = ui->step2_duration->value();
    char mstring[100];
    if (distance == 0) {
        sprintf(mstring, "pause %6.1f", duration);
    }
    else {
        sprintf(mstring, "move %6.1f %6.1f", distance, duration);
    }
    QListWidgetItem *stepItem = new QListWidgetItem;
    stepItem->setText(mstring);
    int currRow = ui->step2_motionQueue->currentRow() + 1;
    ui->step2_motionQueue->insertItem(currRow, stepItem);
    ui->step2_motionQueue->setCurrentRow(currRow);
}

void MainWindow::on_step2_queueBefore_clicked()
{
    double distance = ui->step2_distance->value();
    double duration = ui->step2_duration->value();
    char mstring[100];
    if (distance == 0) {
        sprintf(mstring, "pause %6.1f", duration);
    }
    else {
        sprintf(mstring, "move %6.1f %6.1f", distance, duration);
    }
    QListWidgetItem *stepItem = new QListWidgetItem;
    stepItem->setText(mstring);
    int currRow = ui->step2_motionQueue->currentRow();
    if (currRow < 0) {
        currRow = 0;
        ui->step2_motionQueue->insertItem(currRow, stepItem);
        ui->step2_motionQueue->setCurrentRow(currRow);
    }
    else {
        ui->step2_motionQueue->insertItem(currRow, stepItem);
        ui->step2_motionQueue->setCurrentRow(currRow + 1);
    }
}

void MainWindow::on_step2_loopAfter_clicked()
{
    int loopNum = ++stepperLoops[1];
    int loopCount = ui->step2_loopCount->value();
    char mstring[100];
    sprintf(mstring, "loop %d %d", loopNum, loopCount);
    QListWidgetItem *loopItem_start = new QListWidgetItem;
    loopItem_start->setText(mstring);
    sprintf(mstring, "endloop %d %d", loopNum, loopCount);
    QListWidgetItem *loopItem_end = new QListWidgetItem;
    loopItem_end->setText(mstring);
    int currRow = ui->step2_motionQueue->currentRow() + 1;
    ui->step2_motionQueue->insertItem(currRow, loopItem_start);
    ui->step2_motionQueue->insertItem(currRow+1, loopItem_end);
    ui->step2_motionQueue->setCurrentRow(currRow);
}

void MainWindow::on_step2_loopBefore_clicked()
{
    int loopNum = ++stepperLoops[1];
    int loopCount = ui->step2_loopCount->value();
    char mstring[100];
    sprintf(mstring, "loop %d %d", loopNum, loopCount);
    QListWidgetItem *loopItem_start = new QListWidgetItem;
    loopItem_start->setText(mstring);
    sprintf(mstring, "endloop %d %d", loopNum, loopCount);
    QListWidgetItem *loopItem_end = new QListWidgetItem;
    loopItem_end->setText(mstring);
    int currRow = ui->step1_motionQueue->currentRow();
    if (currRow < 0) {
        currRow = 0;
    }
    ui->step2_motionQueue->insertItem(currRow, loopItem_start);
    ui->step2_motionQueue->insertItem(currRow+1, loopItem_end);
    ui->step2_motionQueue->setCurrentRow(currRow);
}

void MainWindow::on_step2_clearSelected_clicked()
{
    int checkRow, currRow = ui->step2_motionQueue->currentRow();
    if (currRow < 0)
        return;
    int nItems;
    nItems = ui->step2_motionQueue->count();
    QListWidgetItem *currItem = ui->step2_motionQueue->item(currRow);
    QString data = currItem->text();
    int ldepth = 0;
    QListWidgetItem *delWI;
    // Delete a loop start - search forward for the matching loop end to delete...
    if (data.startsWith("loop")) {
        for (checkRow = currRow+1; checkRow < nItems; checkRow++) {
            if (ui->step2_motionQueue->item(checkRow)->text().startsWith("endloop")) {
                if (ldepth-- == 0) break;
            }
            else if (ui->step2_motionQueue->item(checkRow)->text().startsWith("loop")) {
                ldepth++;
            }
        }
        delWI = ui->step2_motionQueue->takeItem(checkRow);
        delete delWI;
        delWI = ui->step2_motionQueue->takeItem(currRow);
        delete delWI;
    }
    // Delete a loop end - search backward for the matching loop start to delete...
    else if (data.startsWith("endloop")) {
        for (checkRow = currRow-1; checkRow >= 0; checkRow--) {
            if (ui->step2_motionQueue->item(checkRow)->text().startsWith("loop")) {
                if (ldepth-- == 0) break;
            }
            else if (ui->step2_motionQueue->item(checkRow)->text().startsWith("endloop")) {
                ldepth++;
            }
        }
        delWI = ui->step2_motionQueue->takeItem(currRow);
        delete delWI;
        delWI = ui->step2_motionQueue->takeItem(checkRow);
        delete delWI;
    }
    // Delete a move command - just delete it...
    else {
        QListWidgetItem *delWI = ui->step2_motionQueue->takeItem(currRow);
        delete delWI;
    }
}

void MainWindow::on_step2_clearAll_clicked()
{
    ui->step2_motionQueue->clear();
    stepperLoops[1] = 0;
}

void MainWindow::on_step2_moveUp_clicked()
{
    int currRow = ui->step2_motionQueue->currentRow();
    if (currRow < 0)
        return;
}

void MainWindow::on_step2_moveDown_clicked()
{
    int currRow = ui->step2_motionQueue->currentRow();
    if (currRow < 0)
        return;
}
