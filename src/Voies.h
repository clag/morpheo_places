#ifndef VOIES_H
#define VOIES_H

#include "Graphe.h"
#include "Logger.h"
#include "Database.h"

#include <QString>
#include <QtSql>

using namespace std;

namespace WayMethods {

    enum methodID {
        ANGLE_MIN = 0,
        ANGLE_SOMME_MIN = 1,
        ANGLE_RANDOM = 2,
        ARCS = 3
    };

    extern int numMethods;

    extern QString MethodeVoies_name[4];

}

class Voies
{
public:

    /** \warning Le graphe soit avoir été construit (do_graphe appelé) avant de le fournir dans ce constructeur */
    Voies(Database* db, Logger* log, Graphe* graphe, WayMethods::methodID methode, double seuil, QString rawTableName, QString directory);

    //---construction des voies
    bool do_Voies();

    //---construction des attributs de voies
    bool do_Att_Voie(bool connexion, bool use, bool inclusion, bool gradient, bool local_access);
    //---construction des attributs de sif
    bool do_Att_Arc();




private:

    //---constitution des paires pour un sommet, selon la méthode
    bool findCouplesAngleMin(int ids);
    bool findCouplesAngleSommeMin(int ids, int N_couples, int pos_couple, int lsom, int pos_ut, QVector< QVector<bool> >* V_utArcs, QVector< QVector<double> >* V_sommeArc, int nb_possib);
    bool findCouplesRandom(int ids);
    bool findCouplesArcs(int ids);

    //---construction des couples
    bool buildCouples();

    //---construction des tableaux membres
    bool buildVectors();

    //---construction de la table VOIES
    bool build_VOIES();

    //---calcul des attributs de voies
    bool calcStructuralite();
    bool calcStructRel();
    bool calcInclusion();
    bool calcLocalAccess();
    bool calcGradient();
    bool calcConnexion();
    bool calcUse();

    //---calcul des attributs d'arcs
    bool calcBruitArcs();

    //---outils
    bool arcInVoie(long ida, long idv);

    //---complétion de la table INFO
    bool insertINFO();

    //---complétion de la table SIF
    bool updateSIF();

    QString m_rawTableName;
    QString m_directory;

    //nombre de couples
    int m_nbCouples;
    //nombre de célibataires
    int m_nbCelibataire;

    // identifiant d'arc (IDA) / arcs couplés avec (IDA' ou 0 pour les impasses) et à quel sommet
    // IDA >> IDS1 | IDA1 | IDS2 | IDA2 | IDV
    QVector< QVector<long> > m_Couples;

    QVector< int > m_Impasses;

    // identifiants des voies par identifiant de sommet
    // ligne IDS >> IDV1, IDV2...
    QVector< QVector<long> > m_SomVoies;

    // identifiants des sommets par identifiant de voie
    // ligne IDV >> IDS1, IDS2.. IDS
    QVector< QVector<long> > m_VoieSommets;

    //nombre de voies
    int m_nbVoies;

    //nb de voies supprimées
    int m_nbVoies_supp;

    //graphe auquel les voies sont rattachées
    Graphe* m_Graphe;

    WayMethods::methodID m_methode;
    double m_seuil_angle;


    //longueur totale du réseau
    float m_length_tot;

    //structuralite totale sur le réseau
    float m_struct_tot;

    //suite d'identifiants d'arcs formant une voie
    // ligne IDV >> IDA1, IDA2.. IDAn
    QVector< QVector<long> > m_VoieArcs;

    // ligne IDV >> IDV1, IDV2.. IDVn
    QVector< QVector<long> > m_VoieVoies;

    //identifiants de la voie correspondant à l'arc
    //ligne IDA >> IDV
    QVector< long > m_ArcVoies;

    Logger* pLogger;
    Database* pDatabase;
};

#endif // VOIES_H
