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
        m_nbSommets = 0;
        m_nbImpasses = 0;
        m_nbPointsAnnexes = 0;
        m_nbIntersections = 0;
        pLogger = log;
        pDatabase = db;
    }
    ~Graphe(){}

    //---construction du graphe
    bool do_Graphe(QString brutArcsTableName = "brut_arcs");

    int getNombreSommets() {return m_nbSommets;}
    int getNombreArcs() {return m_nbArcs;}

    QVector<long>* getArcsOfSommet(int ids) {return &(m_SomArcs[ids]);}
    //bool build_ArcsAzFromSommet(int ids, int buffer);
    bool getSommetsOfArcs(int ida, int* si, int* sf);
    double getAngle(int ids, int ida1, int ida2);
    bool checkAngle(int ids, int ida1, int ida2);

private:

    //---construction de la table SXYZ et de la table IMP
    bool build_SXYZ();

    //---ajout d'un élément à la table SXYZ
    int ajouterSommet(QVariant point);

    //---recherche d'un sommet
    int find_ids(QVariant point);

    //---construction de SIF
    bool build_SIF(QSqlQueryModel* arcs_bruts);

    //---construction de ANGLES
    bool build_ANGLES();

    //---construction des tableaux membres
    bool build_SomArcs();
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
    int m_nbSommets;
    //nombre total d'arcs
    int m_nbArcs;

    //tolérance d'identification des sommets
    float m_toleranceSommets;

    /** \brief identifiants des arcs par identifiant de sommet
     * \abstract La ligne 0 est vide, afin que l'identifiant du sommet corresponde à la ligne dans le vecteur
     * ligne IDS1 >> IDA1, IDA2.. IDAn
     */
    QVector< QVector<long> > m_SomArcs;

    /** \brief identifiants des arcs par identifiant d'arc
     * \abstract La ligne 0 est vide, afin que l'identifiant de l'arc corresponde à la ligne dans le vecteur
     * ligne IDA >> IDA1, IDA2.. IDAn
     */
    QVector< QVector<long> > m_ArcArcs;

    Logger* pLogger;
    Database* pDatabase;

};

#endif // GRAPHE_H
