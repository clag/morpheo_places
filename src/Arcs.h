#ifndef ARCS_H
#define ARCS_H

#include "Graphe.h"
#include "Voies.h"
#include "Logger.h"
#include "Database.h"

#include <QString>
#include <QtSql>

using namespace std;

class Arcs
{
public:
    Arcs(Database* db, Logger* log, Graphe* graphe, Voies* voies, WayMethods::methodID methode, double seuil);

    //méthodes publiques
    bool do_Arcs();

private :
    //graphe auquel les voies sont rattachées
    Graphe* m_Graphe;
    //voies construites
    Voies* m_Voies;

    WayMethods::methodID m_methode;
    double m_seuil_angle;

    //nombre de rues
    int m_nbRues;

    //méthodes privées
    bool build_ARCS();
    bool build_RUES();

    Logger* pLogger;
    Database* pDatabase;
};

#endif // ARCS_H
