#ifndef GRAPHE_H
#define GRAPHE_H

#include <QString>
#include <QtSql>
#include "Logger.h"
#include "Database.h"

using namespace std;
#include <vector>

class Graphe
{
public:

    Graphe(Database* db, Logger* log, float e = 0.001){
        m_toleranceSommets = e;
        m_nbArcs = 0;
        m_nbPlaces = 0;
        m_nbImpasses = 0;
        m_nbIntersections = 0;
        pLogger = log;
        pDatabase = db;
    }
    ~Graphe(){}

    //---construction du graphe
    bool do_Graphe(QString brutArcsTableName = "arcs_nettoyes");

    int getNombrePlaces() {return m_nbPlaces;}
    int getNombreArcs() {return m_nbArcs;}

    QVector<long>* getArcsOfPlace(int idp) {return &(m_PlacesArcs[idp]);}

    bool getPlacesOfArcs(int ida, int* pi, int* pf);
    double getAngle(int idp, int ida1, int ida2);
    bool checkAngle(int idp, int ida1, int ida2);

private:

    //---recherche d'un sommet
    int find_idp(QVariant point);

    //---construction de SIF
    bool build_PIF(QSqlQueryModel* arcs_bruts);

    //---construction de ANGLES
    bool build_PANGLES();

    //---construction des tableaux membres
    bool build_PlacesArcs();
    bool build_ArcArcs();

    //---construction de la table INFO
    bool insertINFO();

    //nombre de sommets intersections
    int m_nbIntersections;
    //nombre de sommets impasses
    int m_nbImpasses;
    //nombre total de points annexes
    int m_nbPointsAnnexes;

    //nombre total de sommets
    int m_nbPlaces;
    //nombre total d'arcs
    int m_nbArcs;

    //tolérance d'identification des sommets
    float m_toleranceSommets;

    /** \brief identifiants des arcs par identifiant de sommet
     * \abstract La ligne 0 est vide, afin que l'identifiant de la place corresponde à la ligne dans le vecteur
     * ligne IDP1 >> IDA1, IDA2.. IDAn
     */
    QVector< QVector<long> > m_PlacesArcs;

    /** \brief identifiants des arcs par identifiant d'arc
     * \abstract La ligne 0 est vide, afin que l'identifiant de l'arc corresponde à la ligne dans le vecteur
     * ligne IDA >> IDA1, IDA2.. IDAn
     */
    QVector< QVector<long> > m_ArcArcs;

    Logger* pLogger;
    Database* pDatabase;

};

#endif // GRAPHE_H
