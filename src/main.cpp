#include "MainWindow.h"
#include "Database.h"
#include "Graphe.h"
#include "Voies.h"
//#include "Sommet.h"
#include "Logger.h"

#include <QApplication>
#include <QtSql>
#include <QTableView>
#include <QTextCodec>

#include <unistd.h>
#include <iostream>
using namespace std;


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    Logger* pLogger = new Logger(true);

    /*	------------------------------- */

    MainWindow oIHM(pLogger);
    oIHM.show();

    a.connect(&a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()));

    return a.exec();
}
