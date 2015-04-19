#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <QObject>
#include <QString>
using namespace std;

class Logger : public QObject
{
    Q_OBJECT

signals:
    void information( QString qsInfo ) ;
    void debug( QString qsFatalError ) ;
    void warning( QString qsWarning ) ;
    void fatal( QString qsFatalError ) ;

public:
    Logger(bool debug = false) {
        debugActive = debug;
    }

    ~Logger(){}

    void ATTENTION(QString message) {
        //cout << "WARNING " << message.toStdString() << endl;
        emit warning(message);
    }
    void ERREUR(QString message) {
        //cout << "  ERROR " << message.toStdString() << endl;
        emit fatal(message);
    }
    void INFO(QString message) {
        //cout << "   INFO " << message.toStdString() << endl;
        emit information(message);
    }
    void DEBUG(QString message) {
        if (debugActive) emit debug(message);
        //cout << "  DEBUG " << message.toStdString() << endl;
    }

    void setDebugActive(bool b) {debugActive = b;}

private:
    // Affiche-t-on les logs de niveau debug
    bool debugActive;
};

#endif // LOGGER_H
