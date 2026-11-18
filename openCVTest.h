#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_openCVTest.h"

class openCVTest : public QMainWindow
{
    Q_OBJECT

public:
    openCVTest(QWidget *parent = nullptr);
    ~openCVTest();
private:
    void initUI();
private:
    Ui::openCVTestClass ui;
};

