#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "ui_mainwindow.h"
#include "Logger.h"
#include "Database.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(Logger* log, QWidget *parent = 0);
    ~MainWindow();

public slots:
    void logDebug( QString qsDebug ) ;
    void logInformation( QString qsInfo ) ;
    void logWarning( QString qsWarning ) ;
    void logFatal( QString qsFatalError ) ;

    void calculate();
    void modify();
    void browse();

    void optionsModification(bool checked);

private:
    Ui::MainWindow *ui;
    Logger* pLogger;
    Database* pDatabase;

    void mettreEnErreur(QString message) {
        pLogger->ERREUR(message);
        ui->statusBar->showMessage("Error !!");
        QApplication::processEvents();
        delete pDatabase;
    }

};

#endif // MAINWINDOW_H
