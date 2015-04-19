#include "Voies.h"

#include <iostream>
#include <fstream>
using namespace std;

#include <math.h>
#define PI 3.1415926535897932384626433832795

namespace WayMethods {

    int numMethods = 4;

    QString MethodeVoies_name[4] = {
        "Choix des couples par angles minimum",
        "Choix des couples par somme minimale des angles",
        "Choix des couples aleatoire",
        "Une voie = un arc"
    };
}

Voies::Voies(Database* db, Logger* log, Graphe* graphe, WayMethods::methodID methode, double seuil, QString rawTableName, QString directory)
{
    pDatabase = db;
    pLogger = log;

    pLogger->INFO(QString("Methode de contruction des voies : %1").arg(WayMethods::MethodeVoies_name[methode]));

    m_Graphe = graphe;
    m_rawTableName = rawTableName;
    m_directory = directory;
    m_seuil_angle = seuil;
    m_methode = methode;
    m_nbCouples = 0;
    m_nbCelibataire = 0;
    m_nbVoies = 0;
    m_length_tot = 0;

    // Les vecteurs dont on connait deja la taille sont dimensionnes
    m_Couples.resize(graphe->getNombreArcs() + 1);
    m_ArcVoies.resize(graphe->getNombreArcs() + 1);
    m_SomVoies.resize(graphe->getNombreSommets() + 1);

    // Les vecteurs qui stockerons les voies n'ont pas une taille connue
    // On ajoute a la place d'indice 0 un vecteur vide pour toujours avoir :
    // indice dans le vecteur = identifiant de l'objet en base
    m_VoieArcs.push_back(QVector<long>(0));
    m_VoieSommets.push_back(QVector<long>(0));
}

//***************************************************************************************************************************************************
//CONSTRUCTION DU TABLEAU DE VECTEURS M_COUPLES
//
//***************************************************************************************************************************************************

bool Voies::findCouplesAngleMin(int ids)
{
    QVector<long> stayingArcs;
    for (int a = 0; a < m_Graphe->getArcsOfSommet(ids)->size(); a++) {
        stayingArcs.push_back(m_Graphe->getArcsOfSommet(ids)->at(a));
    }

    QSqlQueryModel modelAngles;
    QSqlQuery queryAngles;
    queryAngles.prepare("SELECT IDA1, IDA2, ANGLE FROM ANGLES WHERE IDS = :IDS ORDER BY ANGLE ASC;");
    queryAngles.bindValue(":IDS",ids);

    if (! queryAngles.exec()) {
        pLogger->ERREUR(QString("Recuperation arc dans findCouplesAngleMin : %1").arg(queryAngles.lastError().text()));
        return false;
    }

    modelAngles.setQuery(queryAngles);

    pLogger->DEBUG(QString("%1 angles").arg(modelAngles.rowCount()));

    for (int ang = 0; ang < modelAngles.rowCount(); ang++) {

        if (modelAngles.record(ang).value("ANGLE").toDouble() > m_seuil_angle) {
            // A partir de maintenant tous les angles seront au dessus du seuil
            // Tous les arcs qui restent sont celibataires
            for (int a = 0; a < stayingArcs.size(); a++) {
                long ida = stayingArcs.at(a);
                m_Couples[ida].push_back(ids);
                m_Couples[ida].push_back(0); //on l'ajoute comme arc terminal (en couple avec 0)
                m_Impasses.push_back(ida); // Ajout comme impasse
                m_nbCelibataire++;
            }
            return true;
        }

        int a1 = modelAngles.record(ang).value("IDA1").toInt();
        int a2 = modelAngles.record(ang).value("IDA2").toInt();

        int occ1 = stayingArcs.count(a1);
        int occ2 = stayingArcs.count(a2);

        if (occ1 == 0 || occ2 == 0) continue; // Un des arcs a deja ete traite
        if (a1 == a2 && occ1 < 2) continue; // arc boucle deja utilise, mais est en double dans le tableau des arcs

        m_Couples[a1].push_back(ids);
        m_Couples[a1].push_back(a2);
        m_Couples[a2].push_back(ids);
        m_Couples[a2].push_back(a1);

        m_nbCouples++;

        int idx = stayingArcs.indexOf(a1);
        if (idx == -1) {
            pLogger->ERREUR(QString("Pas normal de ne pas avoir l'arc %1 encore disponible au sommet %2").arg(a1).arg(ids));
            return false;
        }
        stayingArcs.remove(idx);
        idx = stayingArcs.indexOf(a2);
        if (idx == -1) {
            pLogger->ERREUR(QString("Pas normal de ne pas avoir l'arc %1 encore disponible au sommet %2").arg(a2).arg(ids));
            return false;
        }
        stayingArcs.remove(idx);
    }

    if (stayingArcs.size() > 1) {
        pLogger->ERREUR(QString("Pas normal d'avoir encore plus d'un arc a cet endroit (sommet %1)").arg(ids));
        for (int sa = 0; sa < stayingArcs.size(); sa++) {
            pLogger->ERREUR(QString("   - Arc %1").arg(stayingArcs.at(sa)));
        }

        return false;
    }

    if (! stayingArcs.isEmpty()) {
        pLogger->DEBUG("Un celibataire !!");
        // On a un nombre impair d'arcs, donc un arc celibataire
        long ida = stayingArcs.at(0);
        m_Couples[ida].push_back(ids);
        m_Couples[ida].push_back(0); //on l'ajoute comme arc terminal (en couple avec 0)
        m_Impasses.push_back(ida); // Ajout comme impasse
        m_nbCelibataire++;
    }

    return true;
}


bool Voies::findCouplesAngleSommeMin(int ids, int N_couples, int pos_couple, int lsom, int pos_ut, QVector< QVector<bool> >* V_utArcs, QVector< QVector<double> >* V_sommeArc, int nb_possib){

    QVector<long>* arcs = m_Graphe->getArcsOfSommet(ids);
    int N_arcs = arcs->size();

    if(pos_couple>N_couples){//ON A FINI LE REMPLISSAGE

        //cout<<"DERNIER ARC"<<endl;

        if(N_arcs%2 == 1){//CAS NB IMPAIR D'ARCS

            for(int r=0; r<N_arcs; r++){

                if (V_utArcs->at(r).at(pos_ut) == false && V_sommeArc->at(lsom).last() == -1){//CAS ARC NON UTILISE

                    (*V_sommeArc)[lsom].last() = arcs->at(r);

                    break;

                }//end if ARC NON UTILISE

            }//end for r


        }//end if IMPAIR

        //TEST
        if(V_sommeArc->at(lsom).last() == -1){
            pLogger->ERREUR(QString("nombre d'arcs insuffisant pour la somme : %1 sur la ligne %2").arg(V_sommeArc->at(lsom)[0]).arg(lsom));
            return false;
        }//end if nbArcs_utilises != N_arcs

        else if(lsom == nb_possib-1){//TOUT A ETE REMPLI
            return true;

        }//end else if

        //TOUT EST OK, ON PASSE A LA LIGNE SUIVANTE

    }
    else{

        //ARC I ----------------------------------------------------------------------
        for(int i=0; i<N_arcs; i++){

            if(! V_utArcs->at(i)[pos_ut]){

                int arc_i = arcs->at(i);

                //ARC J --------------------------------------------------------------
                for(int j=i+1; j<N_arcs; j++){

                    if(! V_utArcs->at(j)[pos_ut]) {

                        int arc_j = arcs->at(j);

                        //STOCKAGE ---------------------------------------------------

                        if(V_sommeArc->at(lsom)[2*pos_couple-1] == -1 && V_sommeArc->at(lsom)[2*pos_couple] == -1){//betonnage
                            double angle = m_Graphe->getAngle(ids, arc_i, arc_j);
                            if (angle < 0) return false;
                            (*V_sommeArc)[lsom][0] += angle;
                            (*V_sommeArc)[lsom][2*pos_couple-1] = arc_i;
                            (*V_sommeArc)[lsom][2*pos_couple] = arc_j;

                            //MEMOIRE DES COUPLES DEJA UTILISES --------------------------
                            for(int r=pos_ut+1; r<N_couples+1; r++){
                                (*V_utArcs)[i][r] = true;
                                (*V_utArcs)[j][r] = true;
                            }//end for reste

                        }//end if betonnage


                        //COMPLETION DES COLONNES PRECEDENTES DE LA LIGNE
                        for(int p=1; p<(2*pos_couple-1); p+=2){
                            if(V_sommeArc->at(lsom)[p] == -1){//NON REMPLI

                                int arc_p=V_sommeArc->at(lsom-1)[p];
                                int arc_p1=V_sommeArc->at(lsom-1)[p+1];

                                double angle = m_Graphe->getAngle(ids, arc_p, arc_p1);
                                if (angle < 0) return false;
                                (*V_sommeArc)[lsom][0] += angle;

                                (*V_sommeArc)[lsom][p] = V_sommeArc->at(lsom-1)[p];
                                (*V_sommeArc)[lsom][p+1] = V_sommeArc->at(lsom-1)[p+1];

                                //MEMOIRE DES COUPLES DEJA UTILISES --------------------------
                                for(int c=0; c<N_arcs; c++){
                                    if(arcs->at(c) == V_sommeArc->at(lsom)[p] || arcs->at(c) == V_sommeArc->at(lsom)[p+1]){
                                        for(int r=pos_ut+1; r<N_couples+1; r++){
                                            (*V_utArcs)[c][r] = true;
                                        }//end for reste
                                    }//end if
                                }//end for c


                            }//end if
                        }//end for completion


                        //NOUVELLE RECHERCHE
                        findCouplesAngleSommeMin(ids, N_couples, pos_couple+1, lsom, pos_ut+1, V_utArcs, V_sommeArc, nb_possib);

                        //MISE A JOUR DE LSOM
                        for(int t=0; t<V_sommeArc->size(); t++){
                            if(V_sommeArc->at(t).back() == -1){
                                //cout<<"lsom reevaluee : "<<t<<endl;
                                lsom = t;
                                break;
                            }//end if
                        }//end for t

                        //MISE A JOUR DE V_UTARCS
                        //REMISE A 0 POUR LES NIVEAUX SUPERIEURS
                        for(int t = 0; t < V_sommeArc->size()-1; t++){
                            if(V_sommeArc->at(t).back() != -1 && V_sommeArc->at(t+1)[0] == 0){//on commence une nouvelle ligne
                                for(int i=0; i<N_arcs; i++){
                                    for(int j=pos_ut+1; j<N_couples+1; j++){
                                        (*V_utArcs)[i][j] = false;
                                    }//end for j
                                }//end for i
                                //cout<<"i et j remis a 0"<<endl;
                                break;
                            }//end if
                        }//end for t


                    }//end if (!arcs_utilises.at(j))

                }//end for j

             }//end if (!arcs_utilises.at(i))

        }//end for i

    }//end else (pos_couple>N_couples)

    //cout<<"-- FIN -- "<<pos_ut<<endl;

    return true;

}//END

// On ne fait pas vraiment un rando, on prend juste les arcs dans l'ordre, sans verifier les angles.
bool Voies::findCouplesRandom(int ids)
{
    QVector<long>* arcs = m_Graphe->getArcsOfSommet(ids);
    int a = 0;
    while (a+1 < arcs->size()) {
        long a1 = arcs->at(a);
        long a2 = arcs->at(a+1);

        double angle = m_Graphe->getAngle(ids, a1, a2);
        if (angle < 0) {
            pLogger->ERREUR(QString("Impossible de trouver l'angle entre les arcs %1 et %2").arg(a1).arg(a2));
            return false;
        }

        if (angle < m_seuil_angle) {
            m_Couples[a1].push_back(ids);
            m_Couples[a1].push_back(a2);
            m_Couples[a2].push_back(ids);
            m_Couples[a2].push_back(a1);
            m_nbCouples++;
        } else {
            // On fait des 2 arcs 2 impasses
            m_Couples[a1].push_back(ids);
            m_Couples[a1].push_back(0); //on l'ajoute comme arc terminal (en couple avec 0)
            m_Impasses.push_back(a1); // Ajout comme impasse
            m_nbCelibataire++;

            m_Couples[a2].push_back(ids);
            m_Couples[a2].push_back(0); //on l'ajoute comme arc terminal (en couple avec 0)
            m_Impasses.push_back(a2); // Ajout comme impasse
            m_nbCelibataire++;
        }
        a += 2;
    }

    if (a < arcs->size() ) {
        // On a un nombre impair d'arcs, donc un arc celibataire
        long ida = arcs->at(a);
        m_Couples[ida].push_back(ids);
        m_Couples[ida].push_back(0); //on l'ajoute comme arc terminal (en couple avec 0)
        m_Impasses.push_back(ida); // Ajout comme impasse
        m_nbCelibataire++;
    }

    return true;
}

// On ne fait un couple que si le sommet est de degres 2
bool Voies::findCouplesArcs(int ids)
{
    QVector<long>* arcs = m_Graphe->getArcsOfSommet(ids);

    if (arcs->size() == 2){

        long a1 = arcs->at(0);
        long a2 = arcs->at(1);

        m_Couples[a1].push_back(ids);
        m_Couples[a1].push_back(a2);
        m_Couples[a2].push_back(ids);
        m_Couples[a2].push_back(a1);
        m_nbCouples++;

    }
    else{

        for (int i=0; i<arcs->size(); i++){
             long a = arcs->at(i);

             m_Couples[a].push_back(ids);
             m_Couples[a].push_back(0); //on l'ajoute comme arc terminal (en couple avec 0)
             m_Impasses.push_back(a); // Ajout comme impasse
             m_nbCelibataire++;
        }

    }

    return true;
}// end findArcs



bool Voies::buildCouples(){

    pLogger->INFO("------------------------- buildCouples START -------------------------");

    QString degreefilename=QString("%1/degree_%2_%3.txt").arg(m_directory).arg(QSqlDatabase::database().databaseName()).arg(m_rawTableName);
    QFile degreeqfile( degreefilename );
    if (! degreeqfile.open(QIODevice::ReadWrite) ) {
        pLogger->ERREUR("Impossible d'ouvrir le fichier où écrire les degrés");
        return false;
    }
    QTextStream degreestream( &degreeqfile );

    for(int ids = 1; ids <= m_Graphe->getNombreSommets(); ids++){
        pLogger->DEBUG(QString("Le sommet IDS %1").arg(ids));

        QVector<long>* arcsOfSommet = m_Graphe->getArcsOfSommet(ids);

        //nombre d'arcs candidats
        int N_arcs = arcsOfSommet->size();

        pLogger->DEBUG(QString("Le sommet IDS %1 se trouve sur %2 arc(s)").arg(ids).arg(N_arcs));
        for(int a = 0; a < N_arcs; a++){
            pLogger->DEBUG(QString("     ID arcs : %1").arg(arcsOfSommet->at(a)));
        }//endfora

        //ECRITURE DU DEGRES DES SOMMETS
        //writing
        degreestream << ids;
        degreestream << " ";
        degreestream << N_arcs;
        degreestream << endl;



        //INITIALISATION

        if (N_arcs > 0) {

            //CALCUL DES COUPLES

            //si 1 seul arc passe par le sommet
            if(N_arcs == 1){
                long ida = arcsOfSommet->at(0);
                m_Couples[ida].push_back(ids);
                m_Couples[ida].push_back(0); //on l'ajoute comme arc terminal (en couple avec 0)
                m_Impasses.push_back(ida); // Ajout comme impasse
                m_nbCelibataire++;

                continue;
            }//endif 1

            //si 2 arcs passent par le sommet : ils sont ensemble !
            if(N_arcs == 2){
                long a1 = arcsOfSommet->at(0);
                long a2 = arcsOfSommet->at(1);

                m_Couples[a1].push_back(ids);
                m_Couples[a1].push_back(a2);
                m_Couples[a2].push_back(ids);
                m_Couples[a2].push_back(a1);
                m_nbCouples++;

                continue;
            }//endif 1

            switch(m_methode) {

                case WayMethods::ANGLE_MIN:
                    if (! findCouplesAngleMin(ids)) return false;
                    break;

                case WayMethods::ANGLE_SOMME_MIN:
                    {
                    //nombre de possibilites
                    int N_couples = N_arcs / 2;
                    float nb_possib=1;
                    int N = N_arcs;
                    while(N>2){
                        nb_possib = nb_possib * ((N*(N-1))/2);
                        N -= 2;
                    }//end while

                    //VECTEUR INDIQUANT SI L'ARC A DEJA ETE UTILISE OU PAS
                    QVector< QVector<bool> >* V_utArcs = new QVector<  QVector<bool> >(N_arcs);

                    for(int i=0; i<N_arcs; i++){
                        (*V_utArcs)[i].resize(N_couples+1);
                        for(int j=0; j<(*V_utArcs)[i].size(); j++){
                            (*V_utArcs)[i][j]=false;
                        }//end for j
                    }//end for i

                    //position dans ce vecteur (colonne)
                    int pos_ut = 0;

                    int pos_couple = 1;
                    int lsom = 0;

                    //VECTEUR DE SOMMES ET D'ARCS
                    QVector< QVector<double> >* V_sommeArc = new QVector<  QVector<double> >(nb_possib);
                    for(int i=0; i<V_sommeArc->size(); i++){
                        (*V_sommeArc)[i].resize(N_arcs+1);
                        (*V_sommeArc)[i][0]=0;
                        for(int j=1; j<V_sommeArc->at(i).size(); j++){
                            (*V_sommeArc)[i][j] = -1;
                        }//end for j
                    }//end for i


                    //RECHERCHE DES PAIRES
                    if (! findCouplesAngleSommeMin(ids, N_couples, pos_couple, lsom, pos_ut, V_utArcs, V_sommeArc, nb_possib)) {
                        return false;
                    }

                    //RESULTAT POUR LE SOMMET S (IDS = S+1)

                    double somme_min = N_couples*m_seuil_angle;
                    int lsom_min = 0;

                    //pour chaque somme calculee
                    for(int lsom = 0; lsom < V_sommeArc->size(); lsom++){

                        // SOMME = V_sommeArc[lsom][0]

                        if(V_sommeArc->at(lsom)[0] < somme_min){

                            lsom_min = lsom;
                            somme_min = V_sommeArc->at(lsom)[0];

                        }//endif nouvelle somme min

                    }//end for lsom

                    if(somme_min < N_couples*m_seuil_angle){

                        for(int i=1; i < V_sommeArc->at(lsom_min).size()-1; i+=2){
                            int a1 = V_sommeArc->at(lsom_min).at(i);
                            int a2 = V_sommeArc->at(lsom_min).at(i+1);

                            m_Couples[a1].push_back(ids);
                            m_Couples[a1].push_back(a2);
                            m_Couples[a2].push_back(ids);
                            m_Couples[a2].push_back(a1);

                            m_nbCouples++;

                        }//end for a

                        if (N_arcs%2 == 1) {
                            int a = V_sommeArc->at(lsom_min).last();
                            m_Couples[a].push_back(ids);
                            m_Couples[a].push_back(0);
                            m_Impasses.push_back(a);
                            m_nbCelibataire++;
                        }

                    } else {
                        QVector<long>* arcs = m_Graphe->getArcsOfSommet(ids);

                        for(int i=0; i<N_arcs; i++){

                            int a = arcs->at(i);
                            m_Couples[a].push_back(ids);
                            m_Couples[a].push_back(0);
                            m_Impasses.push_back(a);
                            m_nbCelibataire++;
                        }//end for i

                    }

                    break;
                    }

                case WayMethods::ANGLE_RANDOM:
                    if (! findCouplesRandom(ids)) return false;
                    break;

                case WayMethods::ARCS:
                    if (! findCouplesArcs(ids)) return false;
                    break;

                default:
                    pLogger->ERREUR("Methode d'appariement des arcs inconnu");
                    return false;
            }

        } else{
            pLogger->ERREUR(QString("Le sommet %1 n'a pas d'arcs, donc pas de couple").arg(ids));
            return false;
        }//end else

    }//end for s

    //close the file
    degreeqfile.close();

    //NOMBRE DE COUPLES
    pLogger->INFO(QString("Nb TOTAL de Couples : %1").arg(m_nbCouples));
    pLogger->INFO(QString("Nb TOTAL de Célibataires : %1").arg(m_nbCelibataire));

    pLogger->INFO("-------------------------- buildCouples END --------------------------");

    // Tests de m_Couples
    for (int a = 1; a < m_Couples.size(); a++) {
        if (m_Couples.at(a).size() != 4) {
            pLogger->ERREUR(QString("L'arc %1 n'a pas une entree valide dans le tableau des couples (que %2 elements au lieu de 4)").arg(a).arg(m_Couples.at(a).size()));
            return false;
        }
        pLogger->DEBUG(QString("Arc %1").arg(a));
        pLogger->DEBUG(QString("    Arcs couples : %1 et %2").arg(m_Couples.at(a).at(1)).arg(m_Couples.at(a).at(3)));
    }

    return true;

}//END build_couples

//***************************************************************************************************************************************************
//CONSTRUCTION DU TABLEAU DES VECTEURS : M_VOIE_ARCS
//
//***************************************************************************************************************************************************

bool Voies::buildVectors(){

    pLogger->INFO("------------------------- buildVectors START -------------------------");

    // On parcourt la liste des impasses pour constituer les voies
    for (int i = 0; i < m_Impasses.size(); i++) {

        int ida_precedent = 0;
        int ida_courant = m_Impasses.at(i);
        if (m_Couples.at(ida_courant).size() == 5) {
            // Cette impasse a deja ete traitee : la voie a deja ete ajoutee
            continue;
        }

        int idv = m_nbVoies + 1;

        pLogger->DEBUG(QString("Construction de la voie %1...").arg(idv));
        pLogger->DEBUG(QString("... a partir de l'arc impasse %1").arg(ida_courant));

        QVector<long> voieA;
        QVector<long> voieS;

        int ids_courant = 0;
        if (m_Couples.at(ida_courant).at(1) == 0) {
            ids_courant = m_Couples.at(ida_courant).at(0);
        } else if (m_Couples.at(ida_courant).at(3) == 0) {
            ids_courant = m_Couples.at(ida_courant).at(2);
        } else {
            pLogger->ERREUR(QString("Mauvaise construction du vecteur des couples : l'arc %1 doit etre une impasse").arg(ida_courant));
            return false;
        }

        voieS.push_back(ids_courant);
        m_SomVoies[ids_courant].push_back(idv);

        while (ida_courant) {

            if (m_Couples.at(ida_courant).size() > 4) {
                pLogger->ERREUR(QString("Mauvaise construction du vecteur des couples : l'arc %1 est deja utilise par la voie %2").arg(ida_courant).arg(m_Couples.at(ida_courant).at(4)));
                return false;
            }

            m_Couples[ida_courant].push_back(idv);
            voieA.push_back(ida_courant);

            if (m_Couples.at(ida_courant).at(1) == ida_precedent) {
                ida_precedent = ida_courant;
                ida_courant = m_Couples.at(ida_precedent).at(3);
                ids_courant = m_Couples.at(ida_precedent).at(2);
            } else if (m_Couples.at(ida_courant).at(3) == ida_precedent) {
                ida_precedent = ida_courant;
                ida_courant = m_Couples.at(ida_precedent).at(1);
                ids_courant = m_Couples.at(ida_precedent).at(0);
            } else {
                pLogger->ERREUR(QString("Mauvaise construction du vecteur des couples : l'arc %1 et l'arc %2 devrait etre couples au sommet %3").arg(ida_courant).arg(ida_precedent).arg(ids_courant));
                pLogger->ERREUR(QString("Impossible de construire la voie %1").arg(idv));
                return false;
            }

            voieS.push_back(ids_courant);
            m_SomVoies[ids_courant].push_back(idv);

        }

        m_VoieArcs.push_back(voieA);
        m_VoieSommets.push_back(voieS);
        m_nbVoies++;
    }

    // Nous avons maintenant traite toutes les impasses. Ils restent potentiellement des arcs a traiter (le cas des boucles)
    for (int ida = 1; ida < m_Couples.size(); ida++) {
        if (m_Couples.at(ida).size() == 5) continue;

        int ida_courant = ida;
        int idv = m_nbVoies + 1;

        pLogger->DEBUG(QString("Construction de la voie %1...").arg(idv));
        pLogger->DEBUG(QString("... a partir de l'arc %1").arg(ida_courant));

        QVector<long> voieA;
        QVector<long> voieS;

        int ida_precedent = m_Couples.at(ida_courant).at(1);
        int ids_courant = m_Couples.at(ida_courant).at(0);

        voieS.push_back(ids_courant);
        m_SomVoies[ids_courant].push_back(idv);

        while (ida_courant) {

            if (m_Couples.at(ida_courant).size() > 4) {
                pLogger->DEBUG("On est retombe sur un arc deja utilise : la boucle est bouclee");
                break;
            }

            m_Couples[ida_courant].push_back(idv);
            voieA.push_back(ida_courant);

            if (m_Couples.at(ida_courant).at(1) == ida_precedent) {
                ida_precedent = ida_courant;
                ida_courant = m_Couples.at(ida_precedent).at(3);
                ids_courant = m_Couples.at(ida_precedent).at(2);
            } else if (m_Couples.at(ida_courant).at(3) == ida_precedent) {
                ida_precedent = ida_courant;
                ida_courant = m_Couples.at(ida_precedent).at(1);
                ids_courant = m_Couples.at(ida_precedent).at(0);
            } else {
                pLogger->ERREUR(QString("Mauvaise construction du vecteur des couples : l'arc %1 et l'arc %2 devrait etre couples au sommet %3").arg(ida_courant).arg(ida_precedent).arg(ids_courant));
                pLogger->ERREUR(QString("Impossible de construire la voie %1").arg(idv));
                return false;
            }

            voieS.push_back(ids_courant);

            m_SomVoies[ids_courant].push_back(idv);

        }

        m_VoieArcs.push_back(voieA);
        m_VoieSommets.push_back(voieS);
        m_nbVoies++;
    }

    pLogger->INFO(QString("Nombre de voies : %1").arg(m_nbVoies));

    // CONSTRUCTION DE VOIE-VOIES
    m_VoieVoies.resize(m_nbVoies + 1);
    for(int idv1 = 1; idv1 < m_nbVoies + 1; idv1++) {

        //on parcours les sommets sur la voie
        for(int s = 0; s < m_VoieSommets[idv1].size(); s++){

            long ids = m_VoieSommets[idv1][s];

            //on cherche les voies passant par ces sommets
            for(int v = 0; v < m_SomVoies[ids].size(); v++){

                long idv2 = m_SomVoies[ids][v];
                if(idv2 != idv1){
                    m_VoieVoies[idv1].push_back(idv2);
                }

            }//end for v (voies passant par les sommets sur la voie)

        }//end for s (sommets sur la voie)

    }//end for idv1 (voie)

    pLogger->INFO("-------------------------- buildVectors END --------------------------");

    for (int a = 1; a < m_Couples.size(); a++) {
        if (m_Couples.at(a).size() != 5) {
            pLogger->ERREUR(QString("L'arc %1 n'a pas une entree valide dans le tableau des couples (que %2 elements au lieu de 5)").arg(a).arg(m_Couples.at(a).size()));
            return false;
        }
    }

    return true;

}//END build_VoieArcs


//***************************************************************************************************************************************************
//CONSTRUCTION DE LA TABLE VOIES EN BDD
//
//***************************************************************************************************************************************************

bool Voies::build_VOIES(){

    bool voiesToDo = true;

    if (! pDatabase->tableExists("VOIES")) {
        voiesToDo = true;
        pLogger->INFO("La table des VOIES n'est pas en base, on la construit !");
    } else {
        // Test si la table VOIES a deja ete construite avec les memes parametres methode + seuil
        QSqlQueryModel estMethodeActive;
        estMethodeActive.setQuery(QString("SELECT * FROM INFO WHERE methode = %1 AND seuil_angle = %2 AND ACTIVE=TRUE").arg((int) m_methode).arg((int) m_seuil_angle));
        if (estMethodeActive.lastError().isValid()) {
            pLogger->ERREUR(QString("Ne peut pas tester l'existance dans table de ce methode + seuil : %1").arg(estMethodeActive.lastError().text()));
            return false;
        }

        if (estMethodeActive.rowCount() > 0) {
            pLogger->INFO("La table des VOIES est deja faite avec cette methode et ce seuil des angles");
            voiesToDo = false;
        } else {
            pLogger->INFO("La table des VOIES est deja faite mais pas avec cette methode ou ce seuil des angles : on la refait");
            // Les voies existent mais n'ont pas ete faites avec la meme methode
            voiesToDo = true;

            if (! pDatabase->dropTable("VOIES")) {
                pLogger->ERREUR("Impossible de supprimer VOIES, pour la recreer avec la bonne methode");
                return false;
            }

            if (! pDatabase->dropTable("DTOPO_VOIES")) {
                pLogger->ERREUR("Impossible de supprimer DTOPO_VOIES, pour la recreer avec la bonne methode");
                return false;
            }
        }
    }

    //SUPPRESSION DE LA TABLE VOIES SI DEJA EXISTANTE
    if (voiesToDo) {

        pLogger->INFO("-------------------------- build_VOIES START ------------------------");

        // MISE A JOUR DE USED DANS ANGLES

        QSqlQuery queryAngle;
        queryAngle.prepare("UPDATE ANGLES SET USED=FALSE;");

        if (! queryAngle.exec()) {
            pLogger->ERREUR(QString("Mise à jour de l'angle (USED=FALSE) : %1").arg(queryAngle.lastError().text()));
            return false;
        }

        for (int a = 1; a < m_Couples.size(); a++) {

            int som1_couple = m_Couples.at(a).at(0);
            int a1 = m_Couples.at(a).at(1);

            if (a1 != 0) {
                if (! m_Graphe->checkAngle(som1_couple, a, a1)) return false;
            }

            int som2_couple = m_Couples.at(a).at(2);
            int a2 = m_Couples.at(a).at(3);
            if (a2 != 0) {
                if (! m_Graphe->checkAngle(som2_couple, a, a2)) return false;
            }
        }

        //CREATION DE LA TABLE VOIES D'ACCUEIL DEFINITIVE

        QSqlQueryModel createVOIES;
        createVOIES.setQuery("CREATE TABLE VOIES ( IDV SERIAL NOT NULL PRIMARY KEY, MULTIGEOM geometry, LENGTH float, NBA integer, NBS integer, NBC integer, NBC_P integer);");

        if (createVOIES.lastError().isValid()) {
            pLogger->ERREUR(QString("createtable_voies : %1").arg(createVOIES.lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        //RECONSTITUTION DE LA GEOMETRIE DES VOIES

        QSqlQueryModel *geometryArcs = new QSqlQueryModel();

        geometryArcs->setQuery("SELECT IDA AS IDA, GEOM AS GEOM FROM SIF;");

        if (geometryArcs->lastError().isValid()) {
            pLogger->ERREUR(QString("req_arcsvoies : %1").arg(geometryArcs->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        int NA = m_Graphe->getNombreArcs();

        if(geometryArcs->rowCount() != NA){
            pLogger->ERREUR("Le nombre de lignes ne correspond pas /!\\ NOMBRE D'ARCS");
            return false;
        }

        QVector<QString> geometryVoies(m_nbVoies + 1);

        //pour toutes les voies
        for (int i = 0; i < geometryArcs->rowCount(); i++) {
            int ida = geometryArcs->record(i).value("IDA").toInt();
            QString geom = geometryArcs->record(i).value("GEOM").toString();
            long idv = m_Couples.at(ida).at(4);
            if (geometryVoies[idv].isEmpty()) {
                geometryVoies[idv] = "'" + geom + "'";
            } else {
                geometryVoies[idv] += ", '" + geom + "'";
            }
        }

        delete geometryArcs;

        QString geomfilename=QString("%1/geom_%2_%3.txt").arg(m_directory).arg(QSqlDatabase::database().databaseName()).arg(m_rawTableName);
        QFile geomqfile( geomfilename );
        if (! geomqfile.open(QIODevice::ReadWrite) ) {
            pLogger->ERREUR("Impossible d'ouvrir le fichier où écrire les géométries");
            return false;
        }
        QTextStream geomstream( &geomqfile );


        // On ajoute toutes les voies, avec la geometrie (multi), le nombre d'arcs, de sommets et la connectivite
        for (int idv = 1; idv <= m_nbVoies; idv++) {

            int nba = m_VoieArcs.at(idv).size();
            int nbs = m_VoieSommets.at(idv).size();

            // CALCUL DU NOMBRE DE CONNEXION DE LA VOIE
            int nbc = 0;
            int nbc_p = 0;

            // Pour chaque sommet appartenant a la voie, on regarde combien d'arcs lui sont connectes
            // On enleve les 2 arcs correspondant a la voie sur laquelle on se trouve
            for(int s = 0;  s < m_VoieSommets[idv].size(); s++){
                nbc += m_Graphe->getArcsOfSommet(m_VoieSommets[idv][s])->size()-2;
            }//end for

            // Dans le cas d'une non boucle, on a enleve a tord 2 arcs pour les fins de voies
            if ( nba + 1 == nbs ) { nbc += 2; nbc_p = nbc - 2; }
            else { nbc_p = nbc;}


            geomstream << idv;
            geomstream << " ";
            geomstream << geometryVoies.at(idv);
            geomstream << endl;



            QString addVoie = QString("INSERT INTO VOIES(IDV, MULTIGEOM, NBA, NBS, NBC, NBC_P) VALUES (%1, ST_LineMerge(ST_Union(ARRAY[%2])) , %3, %4, %5, %6);")
                    .arg(idv).arg(geometryVoies.at(idv)).arg(nba).arg(nbs).arg(nbc).arg(nbc_p);

            QSqlQuery addInVOIES;
            addInVOIES.prepare(addVoie);

            if (! addInVOIES.exec()) {
                pLogger->ERREUR(QString("Impossible d'ajouter la voie %1 dans la table VOIES : %2").arg(idv).arg(addInVOIES.lastError().text()));
                return false;
            }

        }

        geomqfile.close();

        //On ajoute la voie correspondante à l'arc dans SIF

        for (int ida=1; ida < m_Couples.size(); ida++){
            int idv = m_Couples.at(ida).at(4);

            QSqlQuery addIDVAttInSIF;
            addIDVAttInSIF.prepare("UPDATE SIF SET IDV = :IDV WHERE ida = :IDA ;");
            addIDVAttInSIF.bindValue(":IDV",idv);
            addIDVAttInSIF.bindValue(":IDA",ida);

            if (! addIDVAttInSIF.exec()) {
                pLogger->ERREUR(QString("Impossible d'inserer l'identifiant %1 pour l'arc %2").arg(idv).arg(ida));
                pLogger->ERREUR(addIDVAttInSIF.lastError().text());
                return false;
            }

        }

        // On calcule la longueur des voies
        QSqlQuery updateLengthInVOIES;
        updateLengthInVOIES.prepare("UPDATE VOIES SET LENGTH = ST_Length(MULTIGEOM);");

        if (! updateLengthInVOIES.exec()) {
            pLogger->ERREUR(QString("Impossible de calculer la longueur de la voie dans la table VOIES : %1").arg(updateLengthInVOIES.lastError().text()));
            return false;
        }

        // On calcule la connectivite sur la longueur
        if (! pDatabase->add_att_div("VOIES","LOC","LENGTH","NBC")) return false;

        if (! pDatabase->add_att_cl("VOIES", "CL_NBC", "NBC", 10, true)) return false;
        if (! pDatabase->add_att_cl("VOIES", "CL_LOC", "LOC", 10, true)) return false;


        pLogger->INFO("--------------------------- build_VOIES END --------------------------");
    } else {
        pLogger->INFO("------------------------- VOIES already exists -----------------------");
    }

    return true;
}

bool Voies::arcInVoie(long ida, long idv){

    //on parcourt tous les arcs de la voie
    for (int a2 = 0; a2 < m_VoieArcs.at(idv).size(); a2 ++){
        long ida2 = m_VoieArcs.at(idv).at(a2);

        if( ida2 == ida ){ return true; } //on a trouvé sur la voie l'arc en entrée

    }//end for

    return false;    //on n'a pas trouvé l'arc en entrée sur la voie

}//END arcInVoie

bool Voies::updateSIF(){

    //On ajoute la voie correspondante à l'arc dans SIF

    for (int ida=1; ida < m_Couples.size(); ida++){
        int idv = m_Couples.at(ida).at(4);

        QSqlQuery addIDVAttInSIF;
        addIDVAttInSIF.prepare("UPDATE SIF SET IDV = :IDV WHERE ida = :IDA ;");
        addIDVAttInSIF.bindValue(":IDV",idv);
        addIDVAttInSIF.bindValue(":IDA",ida);

        if (! addIDVAttInSIF.exec()) {
            pLogger->ERREUR(QString("Impossible d'inserer l'identifiant %1 pour l'arc %2").arg(idv).arg(ida));
            pLogger->ERREUR(addIDVAttInSIF.lastError().text());
            return false;
        }

    }

    QSqlQueryModel *structFromVOIES = new QSqlQueryModel();

    structFromVOIES->setQuery("SELECT IDV, STRUCT AS STRUCT_VOIE FROM VOIES;");

    if (structFromVOIES->lastError().isValid()) {
        pLogger->ERREUR(QString("Impossible de recuperer les structuralites dans VOIES : %1").arg(structFromVOIES->lastError().text()));
        return false;
    }//end if : test requete QSqlQueryModel

    for(int v = 0; v < m_nbVoies; v++){
        int idv = structFromVOIES->record(v).value("IDV").toInt();
        float struct_voie = structFromVOIES->record(v).value("STRUCT_VOIE").toFloat();


        QSqlQuery addStructAttInSIF;
        addStructAttInSIF.prepare("UPDATE SIF SET STRUCT = :ST WHERE idv = :IDV ;");
        addStructAttInSIF.bindValue(":ST",struct_voie);
        addStructAttInSIF.bindValue(":IDV",idv);

        if (! addStructAttInSIF.exec()) {
            pLogger->ERREUR(QString("Impossible d'inserer la structuralité %1 pour la voie %2").arg(struct_voie).arg(idv));
            pLogger->ERREUR(addStructAttInSIF.lastError().text());
            return false;
        }


    }//end for v

    //SUPPRESSION DE L'OBJET
    delete structFromVOIES;

    return true;
}//END updateSIF


//***************************************************************************************************************************************************
//CALCUL DES ATTRIBUTS
//
//***************************************************************************************************************************************************

bool Voies::calcStructuralite(){

    if (! pDatabase->columnExists("VOIES", "DEGREE") || ! pDatabase->columnExists("VOIES", "RTOPO") || ! pDatabase->columnExists("VOIES", "STRUCT")) {
        pLogger->INFO("---------------------- calcStructuralite START ----------------------");

        // AJOUT DE L'ATTRIBUT DE STRUCTURALITE
        QSqlQueryModel addStructInVOIES;
        addStructInVOIES.setQuery("ALTER TABLE VOIES ADD DEGREE integer, ADD RTOPO float, ADD STRUCT float;");

        if (addStructInVOIES.lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible d'ajouter les attributs de structuralite dans VOIES : %1").arg(addStructInVOIES.lastError().text()));
            return false;
        }

        QSqlQueryModel *lengthFromVOIES = new QSqlQueryModel();

        lengthFromVOIES->setQuery("SELECT IDV, length AS LENGTH_VOIE FROM VOIES;");

        if (lengthFromVOIES->lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible de recuperer les longueurs dans VOIES : %1").arg(lengthFromVOIES->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        if(lengthFromVOIES->rowCount() != m_nbVoies){
            pLogger->ERREUR("Le nombre de lignes ne correspond pas /!\\ NOMBRE DE VOIES ");
            return false;
        }

        //open the file
        QString lengthfilename=QString("%1/length_%2_%3.txt").arg(m_directory).arg(QSqlDatabase::database().databaseName()).arg(m_rawTableName);
        QFile lengthqfile( lengthfilename );
        if (! lengthqfile.open(QIODevice::ReadWrite) ) {
            pLogger->ERREUR("Impossible d'ouvrir le fichier où écrire les longueurs");
            return false;
        }
        QTextStream lengthstream( &lengthqfile );

        float length_voie[m_nbVoies + 1];
        for(int v = 0; v < m_nbVoies; v++){
            int idv = lengthFromVOIES->record(v).value("IDV").toInt();
            length_voie[idv]=lengthFromVOIES->record(v).value("LENGTH_VOIE").toFloat();

            //writing
            lengthstream << idv;
            lengthstream << " ";
            lengthstream << length_voie[idv];
            lengthstream << endl;

            m_length_tot += length_voie[idv];
        }//end for v

        lengthqfile.close();

        //SUPPRESSION DE L'OBJET
        delete lengthFromVOIES;

        pLogger->INFO(QString("LONGUEUR TOTALE DU RESEAU : %1 metres").arg(m_length_tot));


        //TRAITEMENT DES m_nbVoies VOIES

        //open the file dtopofile
        QString dtopofilename=QString("%1/dtopo_%2_%3.txt").arg(m_directory).arg(QSqlDatabase::database().databaseName()).arg(m_rawTableName);
        QFile dtopoqfile( dtopofilename );
        if (! dtopoqfile.open(QIODevice::ReadWrite) ) {
            pLogger->ERREUR("Impossible d'ouvrir le fichier où écrire les distance topographiques");
            return false;
        }
        QTextStream dtopostream( &dtopoqfile );

        dtopostream << m_nbVoies << endl;

        //open the file
        QString adjacencyfilename=QString("%1/adjacency_%2_%3.txt").arg(m_directory).arg(QSqlDatabase::database().databaseName()).arg(m_rawTableName);
        QFile adjacencyqfile( adjacencyfilename );
        if (! adjacencyqfile.open(QIODevice::ReadWrite) ) {
            pLogger->ERREUR("Impossible d'ouvrir le fichier où écrire les adjacences");
            return false;
        }
        QTextStream adjacencystream( &adjacencyqfile );

        int nb_voies_supprimees = 0;

        for(int idv1 = 1; idv1 < m_nbVoies + 1; idv1 ++) {

            pLogger->DEBUG(QString("*** VOIE V : %1").arg(idv1));
            pLogger->DEBUG(QString("*** LENGTH : %1").arg(length_voie[idv1]));

            //dtopofile << idv1 << " ";
            //adjacencyfile << idv1 << " ";

            //INITIALISATION
            float structuralite_v = 0;
            int rayonTopologique_v = 0;
            int dtopo = 0;
            int nb_voiestraitees = 0;
            int nb_voiestraitees_test = 0;
            QVector<int> V_ordreNombre;
            QVector<int> V_ordreLength;
            int dtopo_voies[m_nbVoies + 1];

            for(int i = 0; i < m_nbVoies + 1; i++){
                dtopo_voies[i]=-1;
            }//end for i

            dtopo_voies[idv1] = dtopo;
            nb_voiestraitees = 1;

            V_ordreNombre.push_back(1);
            V_ordreLength.push_back(length_voie[idv1]);

           //-------------------

            //TRAITEMENT
            while(nb_voiestraitees != m_nbVoies) {

                //cout<<endl<<"nb_voiestraitees : "<<nb_voiestraitees<<" / "<<m_nbVoies<<" voies."<<endl;
                nb_voiestraitees_test = nb_voiestraitees;

                //TRAITEMENT DE LA LIGNE ORDRE+1 DANS LES VECTEURS
                V_ordreNombre.push_back(0);
                V_ordreLength.push_back(0);

                //------------------------------------------voie idv2
                for(int idv2 = 1; idv2 < m_nbVoies + 1; idv2++){
                    //on cherche toutes les voies de l'ordre auquel on se trouve
                    if(dtopo_voies[idv2] == dtopo){

                        for (int v2 = 0; v2 < m_VoieVoies.at(idv2).size(); v2 ++) {                            
                            long idv3 = m_VoieVoies.at(idv2).at(v2);

                            // = si la voie n'a pas deja ete traitee
                            if (dtopo_voies[idv3] == -1){

                                dtopo_voies[idv3] = dtopo +1;
                                nb_voiestraitees += 1;
                                V_ordreNombre[dtopo+1] += 1;
                                V_ordreLength[dtopo+1] += length_voie[idv3];

                            }//end if (voie non traitee)
                        }//end for v2

                    }//end if (on trouve les voies de l'ordre souhaite)

                }//end for idv2 (voie)

                //si aucune voie n'a ete trouvee comme connectee a celle qui nous interesse
                if(nb_voiestraitees == nb_voiestraitees_test){
                    //cout<<"Traitement de la partie non connexe : "<<m_nbVoies-nb_voiestraitees<<" voies traitees / "<<m_nbVoies<<" voies."<<endl;


                    int nbvoies_connexe = 0;
                    for(int k=0; k<m_nbVoies; k++){
                        if(dtopo_voies[k]!=-1){
                            nbvoies_connexe += 1;
                        }
                        else{
                            nb_voiestraitees += 1;
                        }//end if
                    }//end for k

                    //SUPPRESSION DES VOIES NON CONNEXES AU GRAPHE PRINCIPAL

                    if(nbvoies_connexe < m_nbVoies/2){

                        QSqlQuery deleteVOIES;
                        deleteVOIES.prepare("DELETE FROM VOIES WHERE idv = :IDV ;");
                        deleteVOIES.bindValue(":IDV",idv1 );

                        nb_voies_supprimees +=1;

                        if (! deleteVOIES.exec()) {
                            pLogger->ERREUR(QString("Impossible de supprimer la voie %1").arg(idv1));
                            pLogger->ERREUR(deleteVOIES.lastError().text());
                            return false;
                        }
                    }//end if nbvoies_connexe < m_nbVoies/2

                    break;

                }// end if nb_voiestraitees == nb_voiestraitees_test

                if(nb_voiestraitees == nb_voiestraitees_test){
                    pLogger->ERREUR(QString("Seulement %1 voies traitees sur %2").arg(nb_voiestraitees).arg(m_nbVoies));
                    return false;
                }//end if

                dtopo += 1;

            }//end while (voies a traitees)

            // On rend persistent toutes les structuralités calculées

            //CALCUL DE LA STRUCTURALITE
            for(int l=0; l<V_ordreLength.size(); l++){

                structuralite_v += l * V_ordreLength.at(l);

            }//end for l (calcul de la simpliest centrality)


            //CALCUL DU RAYON TOPO
            for(int l=0; l<V_ordreNombre.size(); l++){

                rayonTopologique_v += l * V_ordreNombre.at(l);

            }//end for l (calcul de l'ordre de la voie)

            //cout<<endl<<"*******VOIE IDV : "<<idv1<<endl;
            //for(int i=0; i <m_VoieVoies.at(idv1).size(); i++){
            //    cout<<"voie connectee : "<<m_VoieVoies.at(idv1).at(i)<<endl;
            //}

            //INSERTION EN BASE
            QSqlQuery addStructAttInVOIES;
            addStructAttInVOIES.prepare("UPDATE VOIES SET DEGREE = :D, RTOPO = :RT, STRUCT = :S WHERE idv = :IDV ;");
            addStructAttInVOIES.bindValue(":IDV",idv1 );
            addStructAttInVOIES.bindValue(":D",m_VoieVoies.at(idv1).size());
            addStructAttInVOIES.bindValue(":RT",rayonTopologique_v);
            addStructAttInVOIES.bindValue(":S",structuralite_v);

            if (! addStructAttInVOIES.exec()) {
                pLogger->ERREUR(QString("Impossible d'inserer la structuralite %1 et le rayon topo %2 pour la voie %3").arg(structuralite_v).arg(rayonTopologique_v).arg(idv1));
                pLogger->ERREUR(addStructAttInVOIES.lastError().text());
                return false;
            }

            // Here we have the vector

            // dtopofile << "[";
            // adjacencyfile << "[";

            for(int i = 1; i < m_nbVoies + 1; i++){
                if (i <= idv1) {
                    dtopostream << dtopo_voies[i] << " ";
                }

                if(dtopo_voies[i] == -1 || dtopo_voies[i] == 0 || dtopo_voies[i] == 1){
                    adjacencystream << dtopo_voies[i];
                }
                else if(dtopo_voies[i] > 1){
                    adjacencystream << 0;
                }
                else{pLogger->ERREUR(QString("Erreur dans la matrice de distances topologiques"));}

                /*if(i != m_nbVoies){
                    dtopofile << ", ";
                    adjacencyfile << ", ";
                }
                else{*/
                    adjacencystream << " ";
                //}

            }//end for i

           // dtopofile << "]";
           // adjacencyfile << "]";

            dtopostream << endl;
            adjacencystream << endl;

        }//end for idv1

        m_nbVoies_supp = nb_voies_supprimees;


        dtopoqfile.close();
        adjacencyqfile.close();

        if (! pDatabase->add_att_div("VOIES","SOL","STRUCT","LENGTH")) return false;

        if (! pDatabase->add_att_cl("VOIES", "CL_S", "STRUCT", 10, true)) return false;
        if (! pDatabase->add_att_cl("VOIES", "CL_SOL", "SOL", 10, true)) return false;

        if (! pDatabase->add_att_cl("VOIES", "CL_RTOPO", "RTOPO", 10, true)) return false;

        if (! pDatabase->add_att_dif("VOIES", "DIFF_CL", "CL_S", "CL_RTOPO")) return false;

        pLogger->INFO("------------------------ calcStructuralite END ----------------------");
    } else {
        pLogger->INFO("---------------- Struct attributes already in VOIES -----------------");
    }

    return true;

}//END calcStructuralite

bool Voies::calcStructRel(){

    if (! pDatabase->columnExists("VOIES", "RTOPO_SCL0") ||! pDatabase->columnExists("VOIES", "RTOPO_SCL1") || ! pDatabase->columnExists("VOIES", "RTOPO_MCL0") ||! pDatabase->columnExists("VOIES", "RTOPO_MCL1") || ! pDatabase->columnExists("VOIES", "RTOPO_S1")  || ! pDatabase->columnExists("VOIES", "RTOPO_S2") || ! pDatabase->columnExists("VOIES", "RTOPO_S3")  || ! pDatabase->columnExists("VOIES", "RTOPO_S4") || ! pDatabase->columnExists("VOIES", "RTOPO_S5")  || ! pDatabase->columnExists("VOIES", "RTOPO_S6") || ! pDatabase->columnExists("VOIES", "RTOPO_S7")  || ! pDatabase->columnExists("VOIES", "RTOPO_S8") || ! pDatabase->columnExists("VOIES", "RTOPO_S9")  || ! pDatabase->columnExists("VOIES", "RTOPO_S10")|| ! pDatabase->columnExists("VOIES", "RTOPO_M1")  || ! pDatabase->columnExists("VOIES", "RTOPO_M2") || ! pDatabase->columnExists("VOIES", "RTOPO_M3")  || ! pDatabase->columnExists("VOIES", "RTOPO_M4") || ! pDatabase->columnExists("VOIES", "RTOPO_M5")  || ! pDatabase->columnExists("VOIES", "RTOPO_M6") || ! pDatabase->columnExists("VOIES", "RTOPO_M7")  || ! pDatabase->columnExists("VOIES", "RTOPO_M8") || ! pDatabase->columnExists("VOIES", "RTOPO_M9")  || ! pDatabase->columnExists("VOIES", "RTOPO_M10")) {
        pLogger->INFO("---------------------- calcStructRel START ----------------------");

        // AJOUT DE L'ATTRIBUT DE STRUCTURALITE RELATIVE
        QSqlQueryModel addStructRelInVOIES;
        addStructRelInVOIES.setQuery("ALTER TABLE VOIES ADD RTOPO_SCL0 integer, ADD RTOPO_SCL1 integer, ADD RTOPO_MCL0 integer, ADD RTOPO_MCL1 integer, ADD RTOPO_S1 integer, ADD RTOPO_S2 integer, ADD RTOPO_S3 integer, ADD RTOPO_S4 integer, ADD RTOPO_S5 integer, ADD RTOPO_S6 integer, ADD RTOPO_S7 integer, ADD RTOPO_S8 integer, ADD RTOPO_S9 integer, ADD RTOPO_S10 integer, ADD RTOPO_M1 integer, ADD RTOPO_M2 integer, ADD RTOPO_M3 integer, ADD RTOPO_M4 integer, ADD RTOPO_M5 integer, ADD RTOPO_M6 integer, ADD RTOPO_M7 integer, ADD RTOPO_M8 integer, ADD RTOPO_M9 integer, ADD RTOPO_M10 integer;");

        if (addStructRelInVOIES.lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible d'ajouter les attributs de structuralité relative dans VOIES : %1").arg(addStructRelInVOIES.lastError().text()));
            return false;
        }

        QSqlQueryModel *structFromVOIES = new QSqlQueryModel();

        structFromVOIES->setQuery("SELECT IDV, STRUCT AS STRUCT_VOIE, SOL AS MAILL_VOIE, CL_S AS CL_S, CL_SOL AS CL_SOL  FROM VOIES;");

        if (structFromVOIES->lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible de recuperer les structuralites dans VOIES pour calculer l'inclusion: %1").arg(structFromVOIES->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        //CREATION DES TABLEAUX DONNANT LA STRUCTURALITE ET LA CLASSE DE MAILLANCE DE CHAQUE VOIE
        float struct_voie[m_nbVoies + 1];
        float cl_struct_voie[m_nbVoies + 1];
        float struct_voie_sorted10[10];
        float sol_voie[m_nbVoies + 1];
        float cl_sol_voie[m_nbVoies + 1];
        float sol_voie_sorted10[10];

        for(int i = 0; i < m_nbVoies + 1; i++){
            struct_voie[i] = 0;
            sol_voie[i] = 0;
            cl_struct_voie[i] = 0;
            cl_sol_voie[i] = 0;
        }


        for(int v = 0; v < m_nbVoies; v++){
            int idv = structFromVOIES->record(v).value("IDV").toInt();
            struct_voie[idv]=structFromVOIES->record(v).value("STRUCT_VOIE").toFloat();
            sol_voie[idv]=structFromVOIES->record(v).value("MAILL_VOIE").toFloat();
            cl_struct_voie[idv]=structFromVOIES->record(v).value("CL_S").toFloat();
            cl_sol_voie[idv]=structFromVOIES->record(v).value("CL_SOL").toFloat();
            m_struct_tot += struct_voie[idv];
        }//end for v


        for(int i = 0; i < 10; i++){
            struct_voie_sorted10[i] = m_struct_tot;
        }

        for(int i = 0; i < 10; i++){
            sol_voie_sorted10[i] = m_struct_tot * m_length_tot;
        }


        //cout<<"m_struct_tot : "<<m_struct_tot<<endl;

         //POUR LE TABLEAU DES 10 MEILLEURES STRUCTURALITES

        //affichage du tableau des 10 meilleures structuralités
        //for(int v = 1; v < 10; v++){
        //    cout<<"struct_voie_sorted10["<<v<<"] : "<<struct_voie_sorted10[v]<<endl<<endl;
        //}//end for v

        for(int v = 1; v < m_nbVoies + 1; v++){
            if(struct_voie[v] < struct_voie_sorted10[0] && struct_voie[v]!=0 ){
                struct_voie_sorted10[9] =  struct_voie_sorted10[8];
                struct_voie_sorted10[8] =  struct_voie_sorted10[7];
                struct_voie_sorted10[7] =  struct_voie_sorted10[6];
                struct_voie_sorted10[6] =  struct_voie_sorted10[5];
                struct_voie_sorted10[5] =  struct_voie_sorted10[4];
                struct_voie_sorted10[4] =  struct_voie_sorted10[3];
                struct_voie_sorted10[3] =  struct_voie_sorted10[2];
                struct_voie_sorted10[2] =  struct_voie_sorted10[1];
                struct_voie_sorted10[1] =  struct_voie_sorted10[0];
                struct_voie_sorted10[0] =  struct_voie[v];
            }
            else if(struct_voie[v] < struct_voie_sorted10[1] && struct_voie[v]!=0 ){
                struct_voie_sorted10[9] =  struct_voie_sorted10[8];
                struct_voie_sorted10[8] =  struct_voie_sorted10[7];
                struct_voie_sorted10[7] =  struct_voie_sorted10[6];
                struct_voie_sorted10[6] =  struct_voie_sorted10[5];
                struct_voie_sorted10[5] =  struct_voie_sorted10[4];
                struct_voie_sorted10[4] =  struct_voie_sorted10[3];
                struct_voie_sorted10[3] =  struct_voie_sorted10[2];
                struct_voie_sorted10[2] =  struct_voie_sorted10[1];
                struct_voie_sorted10[1] =  struct_voie[v];
            }
            else if(struct_voie[v] < struct_voie_sorted10[2] && struct_voie[v]!=0 ){
                struct_voie_sorted10[9] =  struct_voie_sorted10[8];
                struct_voie_sorted10[8] =  struct_voie_sorted10[7];
                struct_voie_sorted10[7] =  struct_voie_sorted10[6];
                struct_voie_sorted10[6] =  struct_voie_sorted10[5];
                struct_voie_sorted10[5] =  struct_voie_sorted10[4];
                struct_voie_sorted10[4] =  struct_voie_sorted10[3];
                struct_voie_sorted10[3] =  struct_voie_sorted10[2];
                struct_voie_sorted10[2] =  struct_voie[v];
            }
            else if(struct_voie[v] < struct_voie_sorted10[3] && struct_voie[v]!=0 ){
                struct_voie_sorted10[9] =  struct_voie_sorted10[8];
                struct_voie_sorted10[8] =  struct_voie_sorted10[7];
                struct_voie_sorted10[7] =  struct_voie_sorted10[6];
                struct_voie_sorted10[6] =  struct_voie_sorted10[5];
                struct_voie_sorted10[5] =  struct_voie_sorted10[4];
                struct_voie_sorted10[4] =  struct_voie_sorted10[3];
                struct_voie_sorted10[3] =  struct_voie[v];
            }
            else if(struct_voie[v] < struct_voie_sorted10[4] && struct_voie[v]!=0 ){
                struct_voie_sorted10[9] =  struct_voie_sorted10[8];
                struct_voie_sorted10[8] =  struct_voie_sorted10[7];
                struct_voie_sorted10[7] =  struct_voie_sorted10[6];
                struct_voie_sorted10[6] =  struct_voie_sorted10[5];
                struct_voie_sorted10[5] =  struct_voie_sorted10[4];
                struct_voie_sorted10[4] =  struct_voie[v];
            }
            else if(struct_voie[v] < struct_voie_sorted10[5] && struct_voie[v]!=0 ){
                struct_voie_sorted10[9] =  struct_voie_sorted10[8];
                struct_voie_sorted10[8] =  struct_voie_sorted10[7];
                struct_voie_sorted10[7] =  struct_voie_sorted10[6];
                struct_voie_sorted10[6] =  struct_voie_sorted10[5];
                struct_voie_sorted10[5] =  struct_voie[v];
            }
            else if(struct_voie[v] < struct_voie_sorted10[6] && struct_voie[v]!=0 ){
                struct_voie_sorted10[9] =  struct_voie_sorted10[8];
                struct_voie_sorted10[8] =  struct_voie_sorted10[7];
                struct_voie_sorted10[7] =  struct_voie_sorted10[6];
                struct_voie_sorted10[6] =  struct_voie[v];
            }
            else if(struct_voie[v] < struct_voie_sorted10[7] && struct_voie[v]!=0 ){
                struct_voie_sorted10[9] =  struct_voie_sorted10[8];
                struct_voie_sorted10[8] =  struct_voie_sorted10[7];
                struct_voie_sorted10[7] =  struct_voie[v];
            }
            else if(struct_voie[v] < struct_voie_sorted10[8] && struct_voie[v]!=0 ){
                struct_voie_sorted10[9] =  struct_voie_sorted10[8];
                struct_voie_sorted10[8] =  struct_voie[v];
            }
            else if(struct_voie[v] < struct_voie_sorted10[9] && struct_voie[v]!=0 ){
                struct_voie_sorted10[9] =  struct_voie[v];
            }
        }//end for v

        //POUR LE TABLEAU DES 10 MEILLEURES MAILLANCES

        for(int v = 1; v < m_nbVoies + 1; v++){
                 if(sol_voie[v] < sol_voie_sorted10[0] && sol_voie[v]!=0 ){
                     sol_voie_sorted10[9] =  sol_voie_sorted10[8];
                     sol_voie_sorted10[8] =  sol_voie_sorted10[7];
                     sol_voie_sorted10[7] =  sol_voie_sorted10[6];
                     sol_voie_sorted10[6] =  sol_voie_sorted10[5];
                     sol_voie_sorted10[5] =  sol_voie_sorted10[4];
                     sol_voie_sorted10[4] =  sol_voie_sorted10[3];
                     sol_voie_sorted10[3] =  sol_voie_sorted10[2];
                     sol_voie_sorted10[2] =  sol_voie_sorted10[1];
                     sol_voie_sorted10[1] =  sol_voie_sorted10[0];
                     sol_voie_sorted10[0] =  sol_voie[v];
                 }
                 else if(sol_voie[v] < sol_voie_sorted10[1] && sol_voie[v]!=0 ){
                     sol_voie_sorted10[9] =  sol_voie_sorted10[8];
                     sol_voie_sorted10[8] =  sol_voie_sorted10[7];
                     sol_voie_sorted10[7] =  sol_voie_sorted10[6];
                     sol_voie_sorted10[6] =  sol_voie_sorted10[5];
                     sol_voie_sorted10[5] =  sol_voie_sorted10[4];
                     sol_voie_sorted10[4] =  sol_voie_sorted10[3];
                     sol_voie_sorted10[3] =  sol_voie_sorted10[2];
                     sol_voie_sorted10[2] =  sol_voie_sorted10[1];
                     sol_voie_sorted10[1] =  sol_voie[v];
                 }
                 else if(sol_voie[v] < sol_voie_sorted10[2] && sol_voie[v]!=0 ){
                     sol_voie_sorted10[9] =  sol_voie_sorted10[8];
                     sol_voie_sorted10[8] =  sol_voie_sorted10[7];
                     sol_voie_sorted10[7] =  sol_voie_sorted10[6];
                     sol_voie_sorted10[6] =  sol_voie_sorted10[5];
                     sol_voie_sorted10[5] =  sol_voie_sorted10[4];
                     sol_voie_sorted10[4] =  sol_voie_sorted10[3];
                     sol_voie_sorted10[3] =  sol_voie_sorted10[2];
                     sol_voie_sorted10[2] =  sol_voie[v];
                 }
                 else if(sol_voie[v] < sol_voie_sorted10[3] && sol_voie[v]!=0 ){
                     sol_voie_sorted10[9] =  sol_voie_sorted10[8];
                     sol_voie_sorted10[8] =  sol_voie_sorted10[7];
                     sol_voie_sorted10[7] =  sol_voie_sorted10[6];
                     sol_voie_sorted10[6] =  sol_voie_sorted10[5];
                     sol_voie_sorted10[5] =  sol_voie_sorted10[4];
                     sol_voie_sorted10[4] =  sol_voie_sorted10[3];
                     sol_voie_sorted10[3] =  sol_voie[v];
                 }
                 else if(sol_voie[v] < sol_voie_sorted10[4] && sol_voie[v]!=0 ){
                     sol_voie_sorted10[9] =  sol_voie_sorted10[8];
                     sol_voie_sorted10[8] =  sol_voie_sorted10[7];
                     sol_voie_sorted10[7] =  sol_voie_sorted10[6];
                     sol_voie_sorted10[6] =  sol_voie_sorted10[5];
                     sol_voie_sorted10[5] =  sol_voie_sorted10[4];
                     sol_voie_sorted10[4] =  sol_voie[v];
                 }
                 else if(sol_voie[v] < sol_voie_sorted10[5] && sol_voie[v]!=0 ){
                     sol_voie_sorted10[9] =  sol_voie_sorted10[8];
                     sol_voie_sorted10[8] =  sol_voie_sorted10[7];
                     sol_voie_sorted10[7] =  sol_voie_sorted10[6];
                     sol_voie_sorted10[6] =  sol_voie_sorted10[5];
                     sol_voie_sorted10[5] =  sol_voie[v];
                 }
                 else if(sol_voie[v] < sol_voie_sorted10[6] && sol_voie[v]!=0 ){
                     sol_voie_sorted10[9] =  sol_voie_sorted10[8];
                     sol_voie_sorted10[8] =  sol_voie_sorted10[7];
                     sol_voie_sorted10[7] =  sol_voie_sorted10[6];
                     sol_voie_sorted10[6] =  sol_voie[v];
                 }
                 else if(sol_voie[v] < sol_voie_sorted10[7] && sol_voie[v]!=0 ){
                     sol_voie_sorted10[9] =  sol_voie_sorted10[8];
                     sol_voie_sorted10[8] =  sol_voie_sorted10[7];
                     sol_voie_sorted10[7] =  sol_voie[v];
                 }
                 else if(sol_voie[v] < sol_voie_sorted10[8] && sol_voie[v]!=0 ){
                     sol_voie_sorted10[9] =  sol_voie_sorted10[8];
                     sol_voie_sorted10[8] =  sol_voie[v];
                 }
                 else if(sol_voie[v] < sol_voie_sorted10[9] && sol_voie[v]!=0 ){
                     sol_voie_sorted10[9] =  sol_voie[v];
                 }
             }//end for v


        //SUPPRESSION DE L'OBJET
        delete structFromVOIES;

        //affichage du tableau des 10 meilleures structuralités
        //for(int v = 1; v < 10; v++){
        //    cout<<"struct_voie_sorted10["<<v<<"] : "<<struct_voie_sorted10[v]<<endl;
        //}//end for v

        //tableau des distances topologiques
        int dtopo_voies_scl0[m_nbVoies + 1];
        int dtopo_voies_scl1[m_nbVoies + 1];
        int dtopo_voies_mcl0[m_nbVoies + 1];
        int dtopo_voies_mcl1[m_nbVoies + 1];

        int dtopo_voies_m1[m_nbVoies + 1];
        int dtopo_voies_m2[m_nbVoies + 1];
        int dtopo_voies_m3[m_nbVoies + 1];
        int dtopo_voies_m4[m_nbVoies + 1];
        int dtopo_voies_m5[m_nbVoies + 1];
        int dtopo_voies_m6[m_nbVoies + 1];
        int dtopo_voies_m7[m_nbVoies + 1];
        int dtopo_voies_m8[m_nbVoies + 1];
        int dtopo_voies_m9[m_nbVoies + 1];
        int dtopo_voies_m10[m_nbVoies + 1];

        int dtopo_voies_s1[m_nbVoies + 1];
        int dtopo_voies_s2[m_nbVoies + 1];
        int dtopo_voies_s3[m_nbVoies + 1];
        int dtopo_voies_s4[m_nbVoies + 1];
        int dtopo_voies_s5[m_nbVoies + 1];
        int dtopo_voies_s6[m_nbVoies + 1];
        int dtopo_voies_s7[m_nbVoies + 1];
        int dtopo_voies_s8[m_nbVoies + 1];
        int dtopo_voies_s9[m_nbVoies + 1];
        int dtopo_voies_s10[m_nbVoies + 1];

        int nb_voiestraitees_s1 = 0;
        int nb_voiestraitees_s2 = 0;
        int nb_voiestraitees_s3 = 0;
        int nb_voiestraitees_s4 = 0;
        int nb_voiestraitees_s5 = 0;
        int nb_voiestraitees_s6 = 0;
        int nb_voiestraitees_s7 = 0;
        int nb_voiestraitees_s8 = 0;
        int nb_voiestraitees_s9 = 0;
        int nb_voiestraitees_s10 = 0;

        for(int i = 0; i < m_nbVoies + 1; i++){

            //CLASSES MAILLANCE
            //Meshing CL0
            if(cl_sol_voie[i]<1){
                dtopo_voies_mcl0[i]=0;
            }
            else{
                dtopo_voies_mcl0[i]=-1;
            }
            //Meshing CL1
            if(cl_sol_voie[i]<2){
                dtopo_voies_mcl1[i]=0;
            }
            else{
                dtopo_voies_mcl1[i]=-1;
            }

            //CLASSES STRUCTURALITE
            //Structurality CL0
            if(cl_struct_voie[i]<1){
                dtopo_voies_scl0[i]=0;
            }
            else{
                dtopo_voies_scl0[i]=-1;
            }
            //Structurality CL1
            if(cl_struct_voie[i]<2){
                dtopo_voies_scl1[i]=0;
            }
            else{
                dtopo_voies_scl1[i]=-1;
            }


            //Structurality MAX1
            if(struct_voie[i]<=struct_voie_sorted10[0]){
                dtopo_voies_s1[i]=0;
                nb_voiestraitees_s1++;
            }
            else{
                dtopo_voies_s1[i]=-1;
            }
            //Structurality MAX2
            if(struct_voie[i]<=struct_voie_sorted10[1]){
                dtopo_voies_s2[i]=0;
                nb_voiestraitees_s2++;
            }
            else{
                dtopo_voies_s2[i]=-1;
            }
            //Structurality MAX3
            if(struct_voie[i]<=struct_voie_sorted10[2]){
                dtopo_voies_s3[i]=0;
                nb_voiestraitees_s3++;
            }
            else{
                dtopo_voies_s3[i]=-1;
            }
            //Structurality MAX4
            if(struct_voie[i]<=struct_voie_sorted10[3]){
                dtopo_voies_s4[i]=0;
                nb_voiestraitees_s4++;
            }
            else{
                dtopo_voies_s4[i]=-1;
            }
            //Structurality MAX5
            if(struct_voie[i]<=struct_voie_sorted10[4]){
                dtopo_voies_s5[i]=0;
                nb_voiestraitees_s5++;
            }
            else{
                dtopo_voies_s5[i]=-1;
            }
            //Structurality MAX6
            if(struct_voie[i]<=struct_voie_sorted10[5]){
                dtopo_voies_s6[i]=0;
                nb_voiestraitees_s6++;
            }
            else{
                dtopo_voies_s6[i]=-1;
            }
            //Structurality MAX7
            if(struct_voie[i]<=struct_voie_sorted10[6]){
                dtopo_voies_s7[i]=0;
                nb_voiestraitees_s7++;
            }
            else{
                dtopo_voies_s7[i]=-1;
            }
            //Structurality MAX8
            if(struct_voie[i]<=struct_voie_sorted10[7]){
                dtopo_voies_s8[i]=0;
                nb_voiestraitees_s8++;
            }
            else{
                dtopo_voies_s8[i]=-1;
            }
            //Structurality MAX9
            if(struct_voie[i]<=struct_voie_sorted10[8]){
                dtopo_voies_s9[i]=0;
                nb_voiestraitees_s9++;
            }
            else{
                dtopo_voies_s9[i]=-1;
            }
            //Structurality MAX10
            if(struct_voie[i]<=struct_voie_sorted10[9]){
                dtopo_voies_s10[i]=0;
                nb_voiestraitees_s10++;
            }
            else{
                dtopo_voies_s10[i]=-1;
            }

            //Maillance MAX1
            if(sol_voie[i]<=sol_voie_sorted10[0]){
                dtopo_voies_m1[i]=0;
            }
            else{
                dtopo_voies_m1[i]=-1;
            }
            //Maillance MAX2
            if(sol_voie[i]<=sol_voie_sorted10[1]){
                dtopo_voies_m2[i]=0;
            }
            else{
                dtopo_voies_m2[i]=-1;
            }
            //Maillance MAX3
            if(sol_voie[i]<=sol_voie_sorted10[2]){
                dtopo_voies_m3[i]=0;
            }
            else{
                dtopo_voies_m3[i]=-1;
            }
            //Maillance MAX4
            if(sol_voie[i]<=sol_voie_sorted10[3]){
                dtopo_voies_m4[i]=0;
            }
            else{
                dtopo_voies_m4[i]=-1;
            }
            //Maillance MAX5
            if(sol_voie[i]<=sol_voie_sorted10[4]){
                dtopo_voies_m5[i]=0;
            }
            else{
                dtopo_voies_m5[i]=-1;
            }
            //Maillance MAX6
            if(sol_voie[i]<=sol_voie_sorted10[5]){
                dtopo_voies_m6[i]=0;
            }
            else{
                dtopo_voies_m6[i]=-1;
            }
            //Maillance MAX7
            if(sol_voie[i]<=sol_voie_sorted10[6]){
                dtopo_voies_m7[i]=0;
            }
            else{
                dtopo_voies_m7[i]=-1;
            }
            //Maillance MAX8
            if(sol_voie[i]<=sol_voie_sorted10[7]){
                dtopo_voies_m8[i]=0;
            }
            else{
                dtopo_voies_m8[i]=-1;
            }
            //Maillance MAX9
            if(sol_voie[i]<=sol_voie_sorted10[8]){
                dtopo_voies_m9[i]=0;
            }
            else{
                dtopo_voies_m9[i]=-1;
            }
            //Maillance MAX10
            if(sol_voie[i]<=sol_voie_sorted10[9]){
                dtopo_voies_m10[i]=0;
            }
            else{
                dtopo_voies_m10[i]=-1;
            }



        }//end for i


        int dtopo = 0;

        cout<<"m_nbVoies : "<<m_nbVoies<<endl;
        cout<<"m_nbVoies_supp : "<<m_nbVoies_supp<<endl;

        //on parcourt l'ensemble des voies
        while(dtopo != m_nbVoies) {

            for(int idv_ref = 1; idv_ref < m_nbVoies + 1; idv_ref++){

                //on cherche toutes les voies de l'ordre auquel on se trouve POUR MCL0************************************
                if (dtopo_voies_mcl0[idv_ref] == dtopo){

                    //on cherche toutes les voies qui la croise
                    for (int v = 0; v < m_VoieVoies.at(idv_ref).size(); v++) {
                        long idv_t = m_VoieVoies.at(idv_ref).at(v);

                        // = si la voie n'a pas deja ete traitee
                        if (dtopo_voies_mcl0[idv_t] == -1){

                            //on actualise sa distance topologique par rapport à la structure
                            dtopo_voies_mcl0[idv_t] = dtopo +1;

                        }//end if (voie non traitee)
                    }//end for v (toutes les voies qui croisent notre voie de référence)

                }//end if (on trouve les voies de l'ordre souhaite)******************************************************


                //on cherche toutes les voies de l'ordre auquel on se trouve POUR MCL1************************************
                if (dtopo_voies_mcl1[idv_ref] == dtopo){

                    //on cherche toutes les voies qui la croise
                    for (int v = 0; v < m_VoieVoies.at(idv_ref).size(); v++) {
                        long idv_t = m_VoieVoies.at(idv_ref).at(v);

                        // = si la voie n'a pas deja ete traitee
                        if (dtopo_voies_mcl1[idv_t] == -1){

                            //on actualise sa distance topologique par rapport à la structure
                            dtopo_voies_mcl1[idv_t] = dtopo +1;

                        }//end if (voie non traitee)
                    }//end for v (toutes les voies qui croisent notre voie de référence)

                }//end if (on trouve les voies de l'ordre souhaite)******************************************************

                //on cherche toutes les voies de l'ordre auquel on se trouve POUR SCL0************************************
                if (dtopo_voies_scl0[idv_ref] == dtopo){

                    //on cherche toutes les voies qui la croise
                    for (int v = 0; v < m_VoieVoies.at(idv_ref).size(); v++) {
                        long idv_t = m_VoieVoies.at(idv_ref).at(v);

                        // = si la voie n'a pas deja ete traitee
                        if (dtopo_voies_scl0[idv_t] == -1){

                            //on actualise sa distance topologique par rapport à la structure
                            dtopo_voies_scl0[idv_t] = dtopo +1;

                        }//end if (voie non traitee)
                    }//end for v (toutes les voies qui croisent notre voie de référence)

                }//end if (on trouve les voies de l'ordre souhaite)******************************************************


                //on cherche toutes les voies de l'ordre auquel on se trouve POUR SCL1************************************
                if (dtopo_voies_scl1[idv_ref] == dtopo){

                    //on cherche toutes les voies qui la croise
                    for (int v = 0; v < m_VoieVoies.at(idv_ref).size(); v++) {
                        long idv_t = m_VoieVoies.at(idv_ref).at(v);

                        // = si la voie n'a pas deja ete traitee
                        if (dtopo_voies_scl1[idv_t] == -1){

                            //on actualise sa distance topologique par rapport à la structure
                            dtopo_voies_scl1[idv_t] = dtopo +1;

                        }//end if (voie non traitee)
                    }//end for v (toutes les voies qui croisent notre voie de référence)

                }//end if (on trouve les voies de l'ordre souhaite)******************************************************


                //on cherche toutes les voies de l'ordre auquel on se trouve POUR S1************************************
                if (dtopo_voies_s1[idv_ref] == dtopo){

                    //on cherche toutes les voies qui la croise
                    for (int v = 0; v < m_VoieVoies.at(idv_ref).size(); v++) {
                        long idv_t = m_VoieVoies.at(idv_ref).at(v);

                        // = si la voie n'a pas deja ete traitee
                        if (dtopo_voies_s1[idv_t] == -1){

                            //on actualise sa distance topologique par rapport à la structure
                            dtopo_voies_s1[idv_t] = dtopo +1;
                            nb_voiestraitees_s1++;

                        }//end if (voie non traitee)
                    }//end for v (toutes les voies qui croisent notre voie de référence)

                }//end if (on trouve les voies de l'ordre souhaite)******************************************************

                //on cherche toutes les voies de l'ordre auquel on se trouve POUR S2************************************
                if (dtopo_voies_s2[idv_ref] == dtopo){

                    //on cherche toutes les voies qui la croise
                    for (int v = 0; v < m_VoieVoies.at(idv_ref).size(); v++) {
                        long idv_t = m_VoieVoies.at(idv_ref).at(v);

                        // = si la voie n'a pas deja ete traitee
                        if (dtopo_voies_s2[idv_t] == -1){

                            //on actualise sa distance topologique par rapport à la structure
                            dtopo_voies_s2[idv_t] = dtopo +1;
                            nb_voiestraitees_s2++;

                        }//end if (voie non traitee)
                    }//end for v (toutes les voies qui croisent notre voie de référence)

                }//end if (on trouve les voies de l'ordre souhaite)******************************************************

                //on cherche toutes les voies de l'ordre auquel on se trouve POUR S3************************************
                if (dtopo_voies_s3[idv_ref] == dtopo){

                    //on cherche toutes les voies qui la croise
                    for (int v = 0; v < m_VoieVoies.at(idv_ref).size(); v++) {
                        long idv_t = m_VoieVoies.at(idv_ref).at(v);

                        // = si la voie n'a pas deja ete traitee
                        if (dtopo_voies_s3[idv_t] == -1){

                            //on actualise sa distance topologique par rapport à la structure
                            dtopo_voies_s3[idv_t] = dtopo +1;
                            nb_voiestraitees_s3++;

                        }//end if (voie non traitee)
                    }//end for v (toutes les voies qui croisent notre voie de référence)

                }//end if (on trouve les voies de l'ordre souhaite)******************************************************

                //on cherche toutes les voies de l'ordre auquel on se trouve POUR S4************************************
                if (dtopo_voies_s4[idv_ref] == dtopo){

                    //on cherche toutes les voies qui la croise
                    for (int v = 0; v < m_VoieVoies.at(idv_ref).size(); v++) {
                        long idv_t = m_VoieVoies.at(idv_ref).at(v);

                        // = si la voie n'a pas deja ete traitee
                        if (dtopo_voies_s4[idv_t] == -1){

                            //on actualise sa distance topologique par rapport à la structure
                            dtopo_voies_s4[idv_t] = dtopo +1;
                            nb_voiestraitees_s4++;

                        }//end if (voie non traitee)
                    }//end for v (toutes les voies qui croisent notre voie de référence)

                }//end if (on trouve les voies de l'ordre souhaite)******************************************************

                //on cherche toutes les voies de l'ordre auquel on se trouve POUR S5************************************
                if (dtopo_voies_s5[idv_ref] == dtopo){

                    //on cherche toutes les voies qui la croise
                    for (int v = 0; v < m_VoieVoies.at(idv_ref).size(); v++) {
                        long idv_t = m_VoieVoies.at(idv_ref).at(v);

                        // = si la voie n'a pas deja ete traitee
                        if (dtopo_voies_s5[idv_t] == -1){

                            //on actualise sa distance topologique par rapport à la structure
                            dtopo_voies_s5[idv_t] = dtopo +1;
                            nb_voiestraitees_s5++;

                        }//end if (voie non traitee)
                    }//end for v (toutes les voies qui croisent notre voie de référence)

                }//end if (on trouve les voies de l'ordre souhaite)******************************************************

                //on cherche toutes les voies de l'ordre auquel on se trouve POUR S6************************************
                if (dtopo_voies_s6[idv_ref] == dtopo){

                    //on cherche toutes les voies qui la croise
                    for (int v = 0; v < m_VoieVoies.at(idv_ref).size(); v++) {
                        long idv_t = m_VoieVoies.at(idv_ref).at(v);

                        // = si la voie n'a pas deja ete traitee
                        if (dtopo_voies_s6[idv_t] == -1){

                            //on actualise sa distance topologique par rapport à la structure
                            dtopo_voies_s6[idv_t] = dtopo +1;
                            nb_voiestraitees_s6++;

                        }//end if (voie non traitee)
                    }//end for v (toutes les voies qui croisent notre voie de référence)

                }//end if (on trouve les voies de l'ordre souhaite)******************************************************

                //on cherche toutes les voies de l'ordre auquel on se trouve POUR S7************************************
                if (dtopo_voies_s7[idv_ref] == dtopo){

                    //on cherche toutes les voies qui la croise
                    for (int v = 0; v < m_VoieVoies.at(idv_ref).size(); v++) {
                        long idv_t = m_VoieVoies.at(idv_ref).at(v);

                        // = si la voie n'a pas deja ete traitee
                        if (dtopo_voies_s7[idv_t] == -1){

                            //on actualise sa distance topologique par rapport à la structure
                            dtopo_voies_s7[idv_t] = dtopo +1;
                            nb_voiestraitees_s7++;

                        }//end if (voie non traitee)
                    }//end for v (toutes les voies qui croisent notre voie de référence)

                }//end if (on trouve les voies de l'ordre souhaite)******************************************************

                //on cherche toutes les voies de l'ordre auquel on se trouve POUR S8************************************
                if (dtopo_voies_s8[idv_ref] == dtopo){

                    //on cherche toutes les voies qui la croise
                    for (int v = 0; v < m_VoieVoies.at(idv_ref).size(); v++) {
                        long idv_t = m_VoieVoies.at(idv_ref).at(v);

                        // = si la voie n'a pas deja ete traitee
                        if (dtopo_voies_s8[idv_t] == -1){

                            //on actualise sa distance topologique par rapport à la structure
                            dtopo_voies_s8[idv_t] = dtopo +1;
                            nb_voiestraitees_s8++;

                        }//end if (voie non traitee)
                    }//end for v (toutes les voies qui croisent notre voie de référence)

                }//end if (on trouve les voies de l'ordre souhaite)******************************************************

                //on cherche toutes les voies de l'ordre auquel on se trouve POUR S9************************************
                if (dtopo_voies_s9[idv_ref] == dtopo){

                    //on cherche toutes les voies qui la croise
                    for (int v = 0; v < m_VoieVoies.at(idv_ref).size(); v++) {
                        long idv_t = m_VoieVoies.at(idv_ref).at(v);

                        // = si la voie n'a pas deja ete traitee
                        if (dtopo_voies_s9[idv_t] == -1){

                            //on actualise sa distance topologique par rapport à la structure
                            dtopo_voies_s9[idv_t] = dtopo +1;
                            nb_voiestraitees_s9++;

                        }//end if (voie non traitee)
                    }//end for v (toutes les voies qui croisent notre voie de référence)

                }//end if (on trouve les voies de l'ordre souhaite)******************************************************

                //on cherche toutes les voies de l'ordre auquel on se trouve POUR S10************************************
                if (dtopo_voies_s10[idv_ref] == dtopo){

                    //on cherche toutes les voies qui la croise
                    for (int v = 0; v < m_VoieVoies.at(idv_ref).size(); v++) {
                        long idv_t = m_VoieVoies.at(idv_ref).at(v);

                        // = si la voie n'a pas deja ete traitee
                        if (dtopo_voies_s10[idv_t] == -1){

                            //on actualise sa distance topologique par rapport à la structure
                            dtopo_voies_s10[idv_t] = dtopo +1;
                            nb_voiestraitees_s10++;

                        }//end if (voie non traitee)
                    }//end for v (toutes les voies qui croisent notre voie de référence)

                }//end if (on trouve les voies de l'ordre souhaite)******************************************************


                //on cherche toutes les voies de l'ordre auquel on se trouve POUR M1************************************
                if (dtopo_voies_m1[idv_ref] == dtopo){

                    //on cherche toutes les voies qui la croise
                    for (int v = 0; v < m_VoieVoies.at(idv_ref).size(); v++) {
                        long idv_t = m_VoieVoies.at(idv_ref).at(v);

                        // = si la voie n'a pas deja ete traitee
                        if (dtopo_voies_m1[idv_t] == -1){

                            //on actualise sa distance topologique par rapport à la structure
                            dtopo_voies_m1[idv_t] = dtopo +1;

                        }//end if (voie non traitee)
                    }//end for v (toutes les voies qui croisent notre voie de référence)

                }//end if (on trouve les voies de l'ordre souhaite)******************************************************

                //on cherche toutes les voies de l'ordre auquel on se trouve POUR M2************************************
                if (dtopo_voies_m2[idv_ref] == dtopo){

                    //on cherche toutes les voies qui la croise
                    for (int v = 0; v < m_VoieVoies.at(idv_ref).size(); v++) {
                        long idv_t = m_VoieVoies.at(idv_ref).at(v);

                        // = si la voie n'a pas deja ete traitee
                        if (dtopo_voies_m2[idv_t] == -1){

                            //on actualise sa distance topologique par rapport à la structure
                            dtopo_voies_m2[idv_t] = dtopo +1;

                        }//end if (voie non traitee)
                    }//end for v (toutes les voies qui croisent notre voie de référence)

                }//end if (on trouve les voies de l'ordre souhaite)******************************************************

                //on cherche toutes les voies de l'ordre auquel on se trouve POUR M3************************************
                if (dtopo_voies_m3[idv_ref] == dtopo){

                    //on cherche toutes les voies qui la croise
                    for (int v = 0; v < m_VoieVoies.at(idv_ref).size(); v++) {
                        long idv_t = m_VoieVoies.at(idv_ref).at(v);

                        // = si la voie n'a pas deja ete traitee
                        if (dtopo_voies_m3[idv_t] == -1){

                            //on actualise sa distance topologique par rapport à la structure
                            dtopo_voies_m3[idv_t] = dtopo +1;

                        }//end if (voie non traitee)
                    }//end for v (toutes les voies qui croisent notre voie de référence)

                }//end if (on trouve les voies de l'ordre souhaite)******************************************************

                //on cherche toutes les voies de l'ordre auquel on se trouve POUR M4************************************
                if (dtopo_voies_m4[idv_ref] == dtopo){

                    //on cherche toutes les voies qui la croise
                    for (int v = 0; v < m_VoieVoies.at(idv_ref).size(); v++) {
                        long idv_t = m_VoieVoies.at(idv_ref).at(v);

                        // = si la voie n'a pas deja ete traitee
                        if (dtopo_voies_m4[idv_t] == -1){

                            //on actualise sa distance topologique par rapport à la structure
                            dtopo_voies_m4[idv_t] = dtopo +1;

                        }//end if (voie non traitee)
                    }//end for v (toutes les voies qui croisent notre voie de référence)

                }//end if (on trouve les voies de l'ordre souhaite)******************************************************

                //on cherche toutes les voies de l'ordre auquel on se trouve POUR M5************************************
                if (dtopo_voies_m5[idv_ref] == dtopo){

                    //on cherche toutes les voies qui la croise
                    for (int v = 0; v < m_VoieVoies.at(idv_ref).size(); v++) {
                        long idv_t = m_VoieVoies.at(idv_ref).at(v);

                        // = si la voie n'a pas deja ete traitee
                        if (dtopo_voies_m5[idv_t] == -1){

                            //on actualise sa distance topologique par rapport à la structure
                            dtopo_voies_m5[idv_t] = dtopo +1;

                        }//end if (voie non traitee)
                    }//end for v (toutes les voies qui croisent notre voie de référence)

                }//end if (on trouve les voies de l'ordre souhaite)******************************************************

                //on cherche toutes les voies de l'ordre auquel on se trouve POUR M6************************************
                if (dtopo_voies_m6[idv_ref] == dtopo){

                    //on cherche toutes les voies qui la croise
                    for (int v = 0; v < m_VoieVoies.at(idv_ref).size(); v++) {
                        long idv_t = m_VoieVoies.at(idv_ref).at(v);

                        // = si la voie n'a pas deja ete traitee
                        if (dtopo_voies_m6[idv_t] == -1){

                            //on actualise sa distance topologique par rapport à la structure
                            dtopo_voies_m6[idv_t] = dtopo +1;

                        }//end if (voie non traitee)
                    }//end for v (toutes les voies qui croisent notre voie de référence)

                }//end if (on trouve les voies de l'ordre souhaite)******************************************************

                //on cherche toutes les voies de l'ordre auquel on se trouve POUR M7************************************
                if (dtopo_voies_m7[idv_ref] == dtopo){

                    //on cherche toutes les voies qui la croise
                    for (int v = 0; v < m_VoieVoies.at(idv_ref).size(); v++) {
                        long idv_t = m_VoieVoies.at(idv_ref).at(v);

                        // = si la voie n'a pas deja ete traitee
                        if (dtopo_voies_m7[idv_t] == -1){

                            //on actualise sa distance topologique par rapport à la structure
                            dtopo_voies_m7[idv_t] = dtopo +1;

                        }//end if (voie non traitee)
                    }//end for v (toutes les voies qui croisent notre voie de référence)

                }//end if (on trouve les voies de l'ordre souhaite)******************************************************

                //on cherche toutes les voies de l'ordre auquel on se trouve POUR M8************************************
                if (dtopo_voies_m8[idv_ref] == dtopo){

                    //on cherche toutes les voies qui la croise
                    for (int v = 0; v < m_VoieVoies.at(idv_ref).size(); v++) {
                        long idv_t = m_VoieVoies.at(idv_ref).at(v);

                        // = si la voie n'a pas deja ete traitee
                        if (dtopo_voies_m8[idv_t] == -1){

                            //on actualise sa distance topologique par rapport à la structure
                            dtopo_voies_m8[idv_t] = dtopo +1;

                        }//end if (voie non traitee)
                    }//end for v (toutes les voies qui croisent notre voie de référence)

                }//end if (on trouve les voies de l'ordre souhaite)******************************************************

                //on cherche toutes les voies de l'ordre auquel on se trouve POUR M9************************************
                if (dtopo_voies_m9[idv_ref] == dtopo){

                    //on cherche toutes les voies qui la croise
                    for (int v = 0; v < m_VoieVoies.at(idv_ref).size(); v++) {
                        long idv_t = m_VoieVoies.at(idv_ref).at(v);

                        // = si la voie n'a pas deja ete traitee
                        if (dtopo_voies_m9[idv_t] == -1){

                            //on actualise sa distance topologique par rapport à la structure
                            dtopo_voies_m9[idv_t] = dtopo +1;

                        }//end if (voie non traitee)
                    }//end for v (toutes les voies qui croisent notre voie de référence)

                }//end if (on trouve les voies de l'ordre souhaite)******************************************************

                //on cherche toutes les voies de l'ordre auquel on se trouve POUR M10************************************
                if (dtopo_voies_m10[idv_ref] == dtopo){

                    //on cherche toutes les voies qui la croise
                    for (int v = 0; v < m_VoieVoies.at(idv_ref).size(); v++) {
                        long idv_t = m_VoieVoies.at(idv_ref).at(v);

                        // = si la voie n'a pas deja ete traitee
                        if (dtopo_voies_m10[idv_t] == -1){

                            //on actualise sa distance topologique par rapport à la structure
                            dtopo_voies_m10[idv_t] = dtopo +1;

                        }//end if (voie non traitee)
                    }//end for v (toutes les voies qui croisent notre voie de référence)

                }//end if (on trouve les voies de l'ordre souhaite)******************************************************




             }//end for idv_ref (voie)

            dtopo++;

        }//end while



        for(int idv = 1; idv < m_nbVoies + 1; idv++){

            //INSERTION EN BASE
            QSqlQuery addSRAttInVOIES;
            addSRAttInVOIES.prepare("UPDATE VOIES SET RTOPO_SCL0 = :RTOPO_SCL0, RTOPO_SCL1 = :RTOPO_SCL1,RTOPO_MCL0 = :RTOPO_MCL0, RTOPO_MCL1 = :RTOPO_MCL1, RTOPO_S1 = :RTOPO_S1, RTOPO_S2 = :RTOPO_S2, RTOPO_S3 = :RTOPO_S3, RTOPO_S4 = :RTOPO_S4, RTOPO_S5 = :RTOPO_S5, RTOPO_S6 = :RTOPO_S6, RTOPO_S7 = :RTOPO_S7, RTOPO_S8 = :RTOPO_S8, RTOPO_S9 = :RTOPO_S9, RTOPO_S10 = :RTOPO_S10, RTOPO_M1 = :RTOPO_M1, RTOPO_M2 = :RTOPO_M2, RTOPO_M3 = :RTOPO_M3, RTOPO_M4 = :RTOPO_M4, RTOPO_M5 = :RTOPO_M5, RTOPO_M6 = :RTOPO_M6, RTOPO_M7 = :RTOPO_M7, RTOPO_M8 = :RTOPO_M8, RTOPO_M9 = :RTOPO_M9, RTOPO_M10 = :RTOPO_M10 WHERE idv = :IDV ;");
            addSRAttInVOIES.bindValue(":IDV",idv );
            addSRAttInVOIES.bindValue(":RTOPO_SCL0",dtopo_voies_scl0[idv]);
            addSRAttInVOIES.bindValue(":RTOPO_SCL1",dtopo_voies_scl1[idv]);
            addSRAttInVOIES.bindValue(":RTOPO_MCL0",dtopo_voies_mcl0[idv]);
            addSRAttInVOIES.bindValue(":RTOPO_MCL1",dtopo_voies_mcl1[idv]);
            addSRAttInVOIES.bindValue(":RTOPO_S1",dtopo_voies_s1[idv]);
            addSRAttInVOIES.bindValue(":RTOPO_S2",dtopo_voies_s2[idv]);
            addSRAttInVOIES.bindValue(":RTOPO_S3",dtopo_voies_s3[idv]);
            addSRAttInVOIES.bindValue(":RTOPO_S4",dtopo_voies_s4[idv]);
            addSRAttInVOIES.bindValue(":RTOPO_S5",dtopo_voies_s5[idv]);
            addSRAttInVOIES.bindValue(":RTOPO_S6",dtopo_voies_s6[idv]);
            addSRAttInVOIES.bindValue(":RTOPO_S7",dtopo_voies_s7[idv]);
            addSRAttInVOIES.bindValue(":RTOPO_S8",dtopo_voies_s8[idv]);
            addSRAttInVOIES.bindValue(":RTOPO_S9",dtopo_voies_s9[idv]);
            addSRAttInVOIES.bindValue(":RTOPO_S10",dtopo_voies_s10[idv]);
            addSRAttInVOIES.bindValue(":RTOPO_M1",dtopo_voies_m1[idv]);
            addSRAttInVOIES.bindValue(":RTOPO_M2",dtopo_voies_m2[idv]);
            addSRAttInVOIES.bindValue(":RTOPO_M3",dtopo_voies_m3[idv]);
            addSRAttInVOIES.bindValue(":RTOPO_M4",dtopo_voies_m4[idv]);
            addSRAttInVOIES.bindValue(":RTOPO_M5",dtopo_voies_m5[idv]);
            addSRAttInVOIES.bindValue(":RTOPO_M6",dtopo_voies_m6[idv]);
            addSRAttInVOIES.bindValue(":RTOPO_M7",dtopo_voies_m7[idv]);
            addSRAttInVOIES.bindValue(":RTOPO_M8",dtopo_voies_m8[idv]);
            addSRAttInVOIES.bindValue(":RTOPO_M9",dtopo_voies_m9[idv]);
            addSRAttInVOIES.bindValue(":RTOPO_M10",dtopo_voies_m10[idv]);

            if (! addSRAttInVOIES.exec()) {
                pLogger->ERREUR(QString("Impossible d'inserer les structuralité relatives pour la voie %1").arg(idv));
                pLogger->ERREUR(addSRAttInVOIES.lastError().text());
                return false;
            }

        }//end for idv

    } else {
        pLogger->INFO("---------------- StructRel attributes already in VOIES -----------------");
    }

    return true;

}//END calcStructRel

bool Voies::calcConnexion(){

    if (! pDatabase->columnExists("VOIES", "NBCSIN") || ! pDatabase->columnExists("VOIES", "NBCSIN_P")) {
        pLogger->INFO("---------------------- calcConnexion START ----------------------");

        // AJOUT DE L'ATTRIBUT DE CONNEXION == ORTHOGONALITE
        QSqlQueryModel addNbcsinInVOIES;
        addNbcsinInVOIES.setQuery("ALTER TABLE VOIES ADD NBCSIN float, ADD NBCSIN_P float;");

        if (addNbcsinInVOIES.lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible d'ajouter l'attribut nbcsin dans VOIES : %1").arg(addNbcsinInVOIES.lastError().text()));
            return false;
        }

        //pour chaque voie
        for(int idv = 1; idv <= m_nbVoies ; idv++){

            int nba = m_VoieArcs.at(idv).size();
            int nbs = m_VoieSommets.at(idv).size();
            float nbc_sin = 0;
            float nbc_sin_p = 0;

            //pondération de nbc par le SINUS de l'angle
            for (int i=0; i<m_VoieArcs[idv].size()-1; i++){

                int a1 = m_VoieArcs[idv][i];
                int a2 = m_VoieArcs[idv][i+1];
                int sommet = m_VoieSommets[idv][i+1];

                QVector<long> * arcs = m_Graphe->getArcsOfSommet(sommet);

                for(int j=0; j<arcs->size(); j++){

                    int a3 = arcs->at(j);

                    if(a3 == a1 || a3 == a2){
                        continue;
                    }

                    double ang1 = m_Graphe->getAngle(sommet, a1, a3);
                    double ang2 = m_Graphe->getAngle(sommet, a2, a3);
                    double ang;

                    if (ang1 < 0 || ang2 < 0){
                        return false;
                    }

                    if (ang1 < ang2) ang = ang1;
                    else ang = ang2;

                    //entre 0 et 180, le sinus est positif
                    float sin_ang = sin(ang*(PI/180));

                    if (sin_ang < 0){
                        pLogger->ERREUR(QString("Ce n'est pas normal que le sinus soit négatif ! (sin_ang = %1)").arg(sin_ang));
                        return false;
                    }

                    //on ajoute les sinus
                    nbc_sin += sin_ang;

                }

            }//end for


            //ETUDE DES SINUS EN FIN DE VOIE
            if (nba + 1 == nbs){ //on est PAS dans le cas d'une boucle

                float min_ang;
                float sin_angsmin;


                //POUR LE PREMIER SOMMET
                int a1 = m_VoieArcs[idv][0];
                int sommet = m_VoieSommets[idv][0];

                QVector<long> * arcs = m_Graphe->getArcsOfSommet(sommet);

                min_ang = arcs->at(0);
                sin_angsmin = 0;

                for(int j=0; j<arcs->size(); j++){

                    int a3 = arcs->at(j);

                    if(a3 == a1){
                        continue;
                    }

                    double ang1 = m_Graphe->getAngle(sommet, a1, a3);

                    if (ang1 < 0){
                        return false;
                    }

                    //entre 0 et 180, le sinus est positif
                    float sin_ang = sin(ang1*(PI/180));

                    if (sin_ang < 0){
                        pLogger->ERREUR(QString("Ce n'est pas normal que le sinus soit négatif ! (sin_ang = %1)").arg(sin_ang));
                        return false;
                    }

                    //on ajoute les sinus
                    nbc_sin += sin_ang;

                    if (ang1 < min_ang){
                        min_ang = ang1;
                        sin_angsmin = sin_ang;
                    }
                }

                //ON RETIRE L'ARTEFACT D'ALIGNEMENT
                nbc_sin_p -= sin_angsmin;


                //POUR LE DERNIER SOMMET
                a1 = m_VoieArcs[idv].last();
                sommet = m_VoieSommets[idv].last();

                arcs = m_Graphe->getArcsOfSommet(sommet);

                min_ang = arcs->at(0);
                sin_angsmin = 0;

                for(int j=0; j<arcs->size(); j++){

                    int a3 = arcs->at(j);

                    if(a3 == a1){
                        continue;
                    }


                    double ang1 = m_Graphe->getAngle(sommet, a1, a3);

                    if (ang1 < 0){
                        return false;
                    }

                    //entre 0 et 180, le sinus est positif
                    float sin_ang = sin(ang1*(PI/180));

                    if (sin_ang < 0){
                        pLogger->ERREUR(QString("Ce n'est pas normal que le sinus soit négatif ! (sin_ang = %1)").arg(sin_ang));
                        return false;
                    }

                    //on ajoute les sinus
                    nbc_sin += sin_ang;

                    if (ang1 < min_ang){
                        min_ang = ang1;
                        sin_angsmin = sin_ang;
                    }
                }

                //ON RETIRE L'ARTEFACT D'ALIGNEMENT
                nbc_sin_p -= sin_angsmin;


            }//end if (nba + 1 == nbs)
            else{ //On est dans le cas d'une boucle

                int a1 = m_VoieArcs[idv][0];
                int a2 = m_VoieArcs[idv].last();
                int sommet = m_VoieSommets[idv][0];

                QVector<long> * arcs = m_Graphe->getArcsOfSommet(sommet);

                for(int j=0; j<arcs->size(); j++){

                    int a3 = arcs->at(j);

                    if(a3 == a1 || a3 == a2){
                        continue;
                    }

                    double ang1 = m_Graphe->getAngle(sommet, a1, a3);
                    double ang2 = m_Graphe->getAngle(sommet, a2, a3);
                    double ang;

                    if (ang1 < 0 || ang2 < 0){
                        return false;
                    }

                    if (ang1 < ang2) ang = ang1;
                    else ang = ang2;

                    //entre 0 et 180, le sinus est positif
                    float sin_ang = sin(ang*(PI/180));

                    if (sin_ang < 0){
                        pLogger->ERREUR(QString("Ce n'est pas normal que le sinus soit négatif ! (sin_ang = %1)").arg(sin_ang));
                        return false;
                    }

                    //on ajoute les sinus
                    nbc_sin += sin_ang;

                }


            }//end else (nba + 1 == nbs)

            nbc_sin_p += nbc_sin;

            if (nbc_sin_p < 0){
                pLogger->ERREUR(QString("Ce n'est pas normal que le sinus soit négatif ! (nbc_sin_p = %1)").arg(nbc_sin_p));
                return false;
            }

            //pLogger->ATTENTION(QString("nbc_sin_p : %1").arg(nbc_sin_p));
            //pLogger->ATTENTION(QString("idv : %1").arg(idv));
            //pLogger->ATTENTION(QString("nbc_sin : %1").arg(nbc_sin));

            //INSERTION EN BASE

            QString addNbcsin = QString("UPDATE VOIES SET NBCSIN = %1, NBCSIN_P = %2 WHERE idv = %3 ;").arg(nbc_sin).arg(nbc_sin_p).arg(idv);

            QSqlQuery addNbcsinAttInVOIES;
            addNbcsinAttInVOIES.prepare(addNbcsin);

            if (! addNbcsinAttInVOIES.exec()) {
                pLogger->ERREUR(QString("Impossible d'ajouter la voie %1 dans la table VOIES : %2").arg(idv).arg(addNbcsinAttInVOIES.lastError().text()));
                return false;
            }

        }//end for idv


        if (! pDatabase->add_att_div("VOIES", "CONNECT", "NBCSIN", "NBC")) return false;
        if (! pDatabase->add_att_div("VOIES", "CONNECT_P", "NBCSIN_P", "NBC_P")) return false;

        if (! pDatabase->add_att_cl("VOIES", "CL_NBCSIN", "NBCSIN", 10, true)) return false;
        if (! pDatabase->add_att_cl("VOIES", "CL_CONNECT", "CONNECT", 10, true)) return false;
        if (! pDatabase->add_att_cl("VOIES", "CL_CONNECTP", "CONNECT_P", 10, true)) return false;

        //CLASSICATION EN 6 CLASSES
        // 0 - 0.6
        // 0.6 - 0.8
        // 0.8 - 0.9
        // 0.9 - 0.95
        // 0.95 - 0.98
        // 0.98 - max

        // AJOUT DE L'ATTRIBUT DE CLASSIF
        QSqlQueryModel addorthoInVOIES;
        addorthoInVOIES.setQuery("ALTER TABLE VOIES ADD C_ORTHO integer;");

        if (addorthoInVOIES.lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible d'ajouter l'attribut c_ortho dans VOIES : %1").arg(addorthoInVOIES.lastError().text()));
            return false;
        }

        QSqlQueryModel *connectpFromVOIES = new QSqlQueryModel();

        connectpFromVOIES->setQuery("SELECT IDV, CONNECT_P FROM VOIES;");

        if (connectpFromVOIES->lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible de recuperer les connect_p dans VOIES : %1").arg(connectpFromVOIES->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        //CREATION DU TABLEAU DONNANT L'ORTHOGONALITE DE CHAQUE VOIE
        float connectp_voie[m_nbVoies + 1];
        for(int v = 0; v < m_nbVoies; v++){

            int idv = connectpFromVOIES->record(v).value("IDV").toInt();
            connectp_voie[idv] = connectpFromVOIES->record(v).value("CONNECT_P").toFloat();

            int c_connectp;

            if (connectp_voie[idv] < 0.6){c_connectp = 0;}
            else  if (connectp_voie[idv] < 0.8){c_connectp = 1;}
            else  if (connectp_voie[idv] < 0.9){c_connectp = 2;}
            else  if (connectp_voie[idv] < 0.95){c_connectp = 3;}
            else  if (connectp_voie[idv] < 0.98){c_connectp = 4;}
            else {c_connectp = 5;}

            //INSERTION EN BASE

            QString addOrtho = QString("UPDATE VOIES SET C_ORTHO = %1 WHERE idv = %2 ;").arg(c_connectp).arg(idv);

            QSqlQuery addOrthoAttInVOIES;
            addOrthoAttInVOIES.prepare(addOrtho);

            if (! addOrthoAttInVOIES.exec()) {
                pLogger->ERREUR(QString("Impossible d'ajouter pour la voie %1 , l'orhtogonalité : %2").arg(idv).arg(addOrthoAttInVOIES.lastError().text()));
                return false;
            }

        }//end for v




    } else {
        pLogger->INFO("---------------- Connect attributes already in VOIES -----------------");
    }

    return true;

}//END calcConnexion

bool Voies::calcUse(){

    if (! pDatabase->columnExists("VOIES", "USE") || ! pDatabase->columnExists("VOIES", "USE_MLT") || ! pDatabase->columnExists("VOIES", "USE_LGT")) {
        pLogger->INFO("---------------------- calcUse START ----------------------");

        // AJOUT DE L'ATTRIBUT DE USE
        QSqlQueryModel addUseInVOIES;
        addUseInVOIES.setQuery("ALTER TABLE VOIES ADD USE integer, ADD USE_MLT integer, ADD USE_LGT integer;");

        if (addUseInVOIES.lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible d'ajouter les attributs de use dans VOIES : %1").arg(addUseInVOIES.lastError().text()));
            return false;
        }


        //CREATION DU TABLEAU COMPTEUR UNIQUE
        float voie_use[m_nbVoies + 1];
        for(int v = 0; v < m_nbVoies + 1; v++){
            voie_use[v] = 0;
        }//end for v

        //CREATION DU TABLEAU COMPTEUR MULTIPLE
        float voie_useMLT[m_nbVoies + 1];
        for(int v = 0; v < m_nbVoies + 1; v++){
            voie_useMLT[v] = 0;
        }//end for v

        //CREATION DU TABLEAU COMPTEUR DISTANCE
        float voie_useLGT[m_nbVoies + 1];
        for(int v = 0; v < m_nbVoies + 1; v++){
            voie_useLGT[v] = 0;
        }//end for v


        //CALCUL DU NOMBRE DE VOIES DE LA PARTIE CONNEXE
        QSqlQueryModel *nombreVOIES = new QSqlQueryModel();

        nombreVOIES->setQuery("SELECT COUNT(IDV) AS NBV FROM VOIES;");

        if (nombreVOIES->lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible de compter le nombre de VOIES : %1").arg(nombreVOIES->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        if (nombreVOIES->rowCount() != 1) {
            pLogger->ERREUR(QString("Trop de réponses pour le nombre de voies : %1").arg(nombreVOIES->rowCount()));
            return false;
        }//end if : test requete QSqlQueryModel

        int nbVOIESconnexes = nombreVOIES->record(0).value("NBV").toInt();

        //SUPPRESSION DE L'OBJET
        delete nombreVOIES;


        //CREATION DU TABLEAU DONNANT LA LONGUEUR DE CHAQUE VOIE
        QSqlQueryModel *lengthFromVOIES = new QSqlQueryModel();

        lengthFromVOIES->setQuery("SELECT IDV, length AS LENGTH_VOIE FROM VOIES;");

        if (lengthFromVOIES->lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible de recuperer les longueurs dans VOIES : %1").arg(lengthFromVOIES->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        float length_voie[m_nbVoies + 1];

        for(int v = 0; v < m_nbVoies + 1; v++){
            length_voie[v] = -1;
        }//end for v

        for(int v = 0; v < m_nbVoies; v++){
            int idv = lengthFromVOIES->record(v).value("IDV").toInt();
            length_voie[idv] = lengthFromVOIES->record(v).value("LENGTH_VOIE").toFloat();
            m_length_tot += length_voie[idv];
        }//end for v

        //SI length_voie[v] = -1 ici ça veut dire que la voie n'était pas connexe et a été retirée

        //SUPPRESSION DE L'OBJET
        delete lengthFromVOIES;



        //------------------------------------------voie i
        for(int idv1 = 1; idv1 < m_nbVoies + 1; idv1 ++){

            if (length_voie[idv1] != -1){ //La voie idv1 existe dans la table VOIE, elle est connexe au reste

                //CREATION / MAJ DU TABLEAU DE PARENT pour compteur unique
                float voie_parente[m_nbVoies + 1];
                int idv_fille;
                int idv_parent;
                for(int v = 0; v < m_nbVoies + 1; v++){
                    voie_parente[v] = -1;
                }//end for v
                voie_parente[idv1] = 0;

                //CREATION / MAJ DU TABLEAU DE PARENT pour compteur multiple
                float voie_parenteMLT[m_nbVoies + 1];
                int idv_filleMLT;
                int idv_parentMLT;
                for(int v = 0; v < m_nbVoies + 1; v++){
                    voie_parenteMLT[v] = -1;
                }//end for v
                voie_parenteMLT[idv1] = 0;

                //CREATION / MAJ DU TABLEAU DE PARENT pour compteur avec prise en compte de la longueur
                float voie_parenteLGT[m_nbVoies + 1];
                int idv_filleLGT;
                int idv_parentLGT;
                for(int v = 0; v < m_nbVoies + 1; v++){
                    voie_parenteLGT[v] = -1;
                }//end for v
                voie_parenteLGT[idv1] = 0;


                int dtopo = 0;
                int nb_voiestraitees = 0;
                int nb_voiestraitees_test = 0;

                //CREATION / MAJ DU TABLEAU donnant les distances topologiques de toutes les voies par rapport à la voie i
                int dtopo_voies[m_nbVoies + 1];
                for(int i = 0; i < m_nbVoies + 1; i++){
                    dtopo_voies[i] = -1;
                }//end for i

                //CREATION / MAJ DU TABLEAU donnant les longueurs d'accès de toutes les voies par rapport à la voie i
                int lacces_voies[m_nbVoies + 1];
                for(int i = 0; i < m_nbVoies + 1; i++){
                    lacces_voies[i]=0;
                }//end for i

                //traitement de la voie principale en cours (voie i)
                dtopo_voies[idv1] = dtopo;
                nb_voiestraitees = 1;


               while(nb_voiestraitees != nbVOIESconnexes){

                    nb_voiestraitees_test = nb_voiestraitees;

                    //------------------------------------------voie j
                    for(int idv2 = 1; idv2 < m_nbVoies + 1; idv2++){

                        if (length_voie[idv2] != -1){ //La voie idv2 existe dans la table VOIE, elle est connexe au reste

                            //on cherche toutes les voies de l'ordre auquel on se trouve
                            if(dtopo_voies[idv2] == dtopo){

                                for (int v2 = 0; v2 < m_VoieVoies.at(idv2).size(); v2 ++) {
                                    long idv3 = m_VoieVoies.at(idv2).at(v2);

                                     if (length_voie[idv3] != -1){ //La voie idv3 existe dans la table VOIE, elle est connexe au reste

                                        //si on est dans le cas d'un premier chemin le plus cout ou d'un même chemin double
                                        if(dtopo_voies[idv3] == -1 || dtopo_voies[idv3] == dtopo_voies[idv2] + 1){

                                            // on stocke le parent (cas MULTIPLE)
                                            voie_parenteMLT[idv3] = idv2;

                                            // on fait les comptes (cas MULTIPLE)
                                            idv_filleMLT = idv3 ;
                                            idv_parentMLT =  voie_parenteMLT[idv_filleMLT]  ;
                                            if(idv_parentMLT != idv2){
                                                pLogger->ERREUR(QString("Probleme de voie parente !"));
                                                return false;
                                            }
                                            while(voie_parenteMLT[idv_filleMLT] != 0){
                                                voie_useMLT[idv_parentMLT] += 1;
                                                idv_filleMLT = idv_parentMLT;
                                                idv_parentMLT = voie_parenteMLT[idv_filleMLT];

                                                if(idv_parentMLT == -1){
                                                    pLogger->ERREUR(QString("Probleme de voie parente non remplie ! (idv_parentMLT = %1)").arg(idv_parentMLT));
                                                    return false;
                                                }
                                            }

                                        }

                                        // = SI LA VOIE A DEJA ETE TRAITEE
                                        if (dtopo_voies[idv3] != -1){

                                            //on compare les longueurs d'accès
                                            if(lacces_voies[idv3] > length_voie[idv2] + lacces_voies[idv2]){
                                                //on a trouvé un chemin plus court en distance ! (même si équivalent plus grand en distance topologique)

                                                //pLogger->ATTENTION(QString("Chemin plus court en distance pour la voie %1 par rapport à la voie %2 : %3 (plus court que %4)").arg(idv3).arg(idv1).arg(length_voie[idv2] + lacces_voies[idv2]).arg(lacces_voies[idv3]));

                                                idv_filleLGT = idv3 ;
                                                idv_parentLGT = voie_parenteLGT[idv_filleLGT];


                                                //si ce n'est pas la première fois
                                                if(idv_parentLGT != -1){

                                                    //pLogger->ATTENTION(QString("Ce n'est pas la première fois qu'un chemin plus court en distance est trouvé !"));


                                                    //IL FAUT ENLEVER L'INFO DANS USE AJOUTEE A TORD
                                                    //REMISE A NIVEAU DU USE
                                                    while(idv_parentLGT != 0){
                                                        voie_useLGT[idv_parentLGT] -= 1;
                                                        idv_filleLGT = idv_parentLGT;
                                                        idv_parentLGT = voie_parente[idv_filleLGT];

                                                        if(idv_parentLGT == -1){
                                                            pLogger->ERREUR(QString("REMISE A NIVEAU DU USE : Probleme de voie parente non remplie ! (idv_parentLGT = %1)").arg(idv_parentLGT));
                                                            return false;
                                                        }
                                                    }

                                                    //IL FAUT SUPPRIMER L'ANCIEN CHEMIN AJOUTE A TORD
                                                    //REMONTEE JUSQUA L'ANCETRE COMMUN

                                                 }

                                                //IL FAUT STOCKER LE NOUVEAU

                                                // on stocke le parent (cas DISTANCE MIN)
                                                voie_parenteLGT[idv3] = idv2;

                                                // on fait les comptes (cas DISTANCE MIN)
                                                idv_filleLGT = idv3 ;
                                                idv_parentLGT =  voie_parenteLGT[idv_filleLGT]  ;

                                                while(idv_parentLGT != 0){
                                                    voie_useLGT[idv_parentLGT] += 1;
                                                    idv_filleLGT = idv_parentLGT;
                                                    idv_parentLGT = voie_parente[idv_filleLGT];

                                                    if(idv_parentLGT == -1){
                                                       pLogger->ERREUR(QString("Probleme de voie parente non remplie ! (idv_parentLGT = %1)").arg(idv_parentLGT));
                                                       return false;
                                                    }
                                                }


                                                /*else{ pLogger->ERREUR(QString("idv_parentLGT = %1)").arg(idv_parentLGT));
                                                      return false;
                                                };*/

                                            }// end if nouveau chemin plus court en distance

                                        }//end if déjà traitée


                                        // = SI LA VOIE N'A PAS DEJA ETE TRAITEE
                                        if (dtopo_voies[idv3] == -1){

                                            dtopo_voies[idv3] = dtopo +1;
                                            lacces_voies[idv3] = length_voie[idv2] + lacces_voies[idv2];
                                            nb_voiestraitees += 1;


                                            // on stocke le parent UNIQUE
                                            voie_parente[idv3] = idv2;

                                            // on fait les comptes UNIQUE
                                            idv_fille = idv3 ;
                                            idv_parent =  voie_parente[idv_fille]  ;
                                            if(idv_parent != idv2){
                                                pLogger->ERREUR(QString("Probleme de voie parente !"));
                                                return false;
                                            }
                                            while(voie_parente[idv_fille] != 0){
                                                voie_use[idv_parent] += 1;
                                                idv_fille = idv_parent;
                                                idv_parent = voie_parente[idv_fille];

                                                if(idv_parent == -1){
                                                    pLogger->ERREUR(QString("Probleme de voie parente non remplie ! (idv_parent = %1)").arg(idv_parent));
                                                    return false;
                                                }
                                            }

                                            /*//USE LGT = USE TOPO
                                            for(int v = 0; v < m_nbVoies + 1; v++){
                                                voie_parenteLGT[v] = voie_parente[v];
                                                voie_useLGT[v] = voie_use[v];
                                            }//end for v*/

                                        }//end if (voie non traitee)

                                     }//end if

                                }//end for v2 (idv3)

                            }//end if (on trouve les voies de l'ordre souhaite)

                        }//end if

                    }//end for idv2 : voie j

                    if(nb_voiestraitees == nb_voiestraitees_test && nb_voiestraitees != nbVOIESconnexes){
                        pLogger->ERREUR(QString("Seulement %1 voies traitees sur %2 pour idv %3").arg(nb_voiestraitees).arg(nbVOIESconnexes).arg(idv1));
                        return false;
                    }//end if

                    dtopo += 1;

                }//end while (voies a traitees)

            }//end if

        }//end for idv1

        //CALCUL DE USE
        for(int idv = 1; idv < m_nbVoies + 1; idv++){

            int use_v = voie_use[idv];
            int useMLT_v = voie_useMLT[idv];

            int useLGT_v = voie_useLGT[idv];
            if(voie_useLGT[idv] == 0){useLGT_v = voie_use[idv];}

            QSqlQuery addUseAttInVOIES;
            addUseAttInVOIES.prepare("UPDATE VOIES SET USE = :USE, USE_MLT = :USE_MLT, USE_LGT = :USE_LGT WHERE idv = :IDV ;");
            addUseAttInVOIES.bindValue(":IDV", idv );
            addUseAttInVOIES.bindValue(":USE",use_v);
            addUseAttInVOIES.bindValue(":USE_MLT",useMLT_v);
            addUseAttInVOIES.bindValue(":USE_LGT",useLGT_v);

            if (! addUseAttInVOIES.exec()) {
                pLogger->ERREUR(QString("Impossible d'inserer l'a structuralite'attribut use %1 pour la voie %2").arg(use_v).arg(idv));
                pLogger->ERREUR(addUseAttInVOIES.lastError().text());
                return false;
            }

        }//end for idv

        if (! pDatabase->add_att_cl("VOIES", "CL_USE", "USE", 10, true)) return false;

        if (! pDatabase->add_att_cl("VOIES", "CL_USEMLT", "USE_MLT", 10, true)) return false;

        if (! pDatabase->add_att_cl("VOIES", "CL_USELGT", "USE_LGT", 10, true)) return false;

        pLogger->INFO("------------------------ calcUse END ----------------------");

    } else {
        pLogger->INFO("---------------- Use attributes already in VOIES -----------------");
    }

    return true;

}//END calcUse

bool Voies::calcInclusion(){

    if (! pDatabase->columnExists("VOIES", "INCL") || ! pDatabase->columnExists("VOIES", "INCL_MOY")) {
        pLogger->INFO("---------------------- calcInclusion START ----------------------");

        // AJOUT DE L'ATTRIBUT D'INCLUSION
        QSqlQueryModel addInclInVOIES;
        addInclInVOIES.setQuery("ALTER TABLE VOIES ADD INCL float, ADD INCL_MOY float;");

        if (addInclInVOIES.lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible d'ajouter les attributs d'inclusion dans VOIES : %1").arg(addInclInVOIES.lastError().text()));
            return false;
        }

        QSqlQueryModel *structFromVOIES = new QSqlQueryModel();

        structFromVOIES->setQuery("SELECT IDV, STRUCT AS STRUCT_VOIE FROM VOIES;");

        if (structFromVOIES->lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible de recuperer les structuralites dans VOIES pour calculer l'inclusion: %1").arg(structFromVOIES->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        //CREATION DU TABLEAU DONNANT LA STRUCTURALITE DE CHAQUE VOIE
        float struct_voie[m_nbVoies + 1];

        for(int i = 0; i < m_nbVoies + 1; i++){
            struct_voie[i] = 0;
        }

        for(int v = 0; v < m_nbVoies; v++){
            int idv = structFromVOIES->record(v).value("IDV").toInt();
            struct_voie[idv]=structFromVOIES->record(v).value("STRUCT_VOIE").toFloat();
            m_struct_tot += struct_voie[idv];
        }//end for v

        //SUPPRESSION DE L'OBJET
        delete structFromVOIES;

        for(int idv1 = 1; idv1 < m_nbVoies + 1; idv1++){

            float inclusion = 0;
            float inclusion_moy = 0;

            //on cherche toutes les voies connectées
            for (int v = 0; v < m_VoieVoies.at(idv1).size(); v ++) {
                long idv2 = m_VoieVoies.at(idv1).at(v);

                inclusion += struct_voie[idv2];
            }//end for idv2


            if(m_VoieVoies.at(idv1).size() != 0) {inclusion_moy = inclusion / m_VoieVoies.at(idv1).size();}
            else {inclusion_moy = 0;}

            //INSERTION EN BASE
            QSqlQuery addInclAttInVOIES;
            addInclAttInVOIES.prepare("UPDATE VOIES SET INCL = :INCL, INCL_MOY = :INCL_MOY WHERE idv = :IDV ;");
            addInclAttInVOIES.bindValue(":IDV",idv1 );
            addInclAttInVOIES.bindValue(":INCL",inclusion);
            addInclAttInVOIES.bindValue(":INCL_MOY",inclusion_moy);

            if (! addInclAttInVOIES.exec()) {
                pLogger->ERREUR(QString("Impossible d'inserer l'inclusion %1 et l'inclusion moyenne %2 pour la voie %3").arg(inclusion).arg(inclusion_moy).arg(idv1));
                pLogger->ERREUR(addInclAttInVOIES.lastError().text());
                return false;
            }

        }//end for idv1

        if (! pDatabase->add_att_cl("VOIES", "CL_INCL", "INCL", 10, true)) return false;
        if (! pDatabase->add_att_cl("VOIES", "CL_INCLMOY", "INCL_MOY", 10, true)) return false;

        pLogger->INFO(QString("STRUCTURALITE TOTALE SUR LE RESEAU : %1").arg(m_struct_tot));

    } else {
        pLogger->INFO("---------------- Incl attributes already in VOIES -----------------");
    }

    return true;

}//END calcInclusion

bool Voies::calcLocalAccess(){

    if (! pDatabase->columnExists("VOIES", "LOCAL_ACCESS1") || ! pDatabase->columnExists("VOIES", "LOCAL_ACCESS2") || ! pDatabase->columnExists("VOIES", "LOCAL_ACCESS3")) {
        pLogger->INFO("---------------------- calcLocalAccess START ----------------------");

        // AJOUT DE L'ATTRIBUT D'ACCESSIBILITE LOCALE
        QSqlQueryModel addLAInVOIES;
        addLAInVOIES.setQuery("ALTER TABLE VOIES ADD LOCAL_ACCESS1 integer, ADD LOCAL_ACCESS2 integer, ADD LOCAL_ACCESS3 integer;");

        if (addLAInVOIES.lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible d'ajouter les attributs d'accessibilité locale dans VOIES : %1").arg(addLAInVOIES.lastError().text()));
            return false;
        }

        QSqlQueryModel *degreeFromVOIES = new QSqlQueryModel();

        degreeFromVOIES->setQuery("SELECT IDV, DEGREE AS DEGREE_VOIE FROM VOIES;");

        if (degreeFromVOIES->lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible de recuperer les degres dans VOIES pour calculer l'acessibilite locale: %1").arg(degreeFromVOIES->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        //CREATION DU TABLEAU DONNANT LA STRUCTURALITE DE CHAQUE VOIE
        float degree_voie[m_nbVoies + 1];

        for(int i = 0; i < m_nbVoies + 1; i++){
            degree_voie[i] = 0;
        }

        for(int v = 0; v < m_nbVoies; v++){
            int idv = degreeFromVOIES->record(v).value("IDV").toInt();
            degree_voie[idv]=degreeFromVOIES->record(v).value("DEGREE_VOIE").toFloat();
        }//end for v

        //SUPPRESSION DE L'OBJET
        delete degreeFromVOIES;

        for(int idv1 = 1; idv1 < m_nbVoies + 1; idv1++){

            float loc_acc_1 = 0;
            float loc_acc_2 = 0;
            float loc_acc_3 = 0;

            //on cherche toutes les voies connectées à idv1
            for (int v2 = 0; v2 < m_VoieVoies.at(idv1).size(); v2 ++) {
                long idv2 = m_VoieVoies.at(idv1).at(v2);
                loc_acc_1 += degree_voie[idv2];
                loc_acc_2 += degree_voie[idv2];
                loc_acc_3 += degree_voie[idv2];

                //on cherche toutes les voies connectées à idv2
                for (int v3 = 0; v3 < m_VoieVoies.at(idv2).size(); v3 ++) {
                    long idv3 = m_VoieVoies.at(idv2).at(v3);
                    loc_acc_2 += degree_voie[idv3];
                    loc_acc_3 += degree_voie[idv3];

                    //on cherche toutes les voies connectées à idv3
                    for (int v4 = 0; v4 < m_VoieVoies.at(idv3).size(); v4 ++) {
                        long idv4= m_VoieVoies.at(idv3).at(v4);
                        loc_acc_3 += degree_voie[idv4];

                    }//end for v4

                }//end for v3

            }//end for v2


            //INSERTION EN BASE
            QSqlQuery addInclAttInVOIES;
            addInclAttInVOIES.prepare("UPDATE VOIES SET LOCAL_ACCESS1 = :LA1, LOCAL_ACCESS2 = :LA2, LOCAL_ACCESS3 = :LA3 WHERE idv = :IDV ;");
            addInclAttInVOIES.bindValue(":IDV",idv1 );
            addInclAttInVOIES.bindValue(":LA1",loc_acc_1);
            addInclAttInVOIES.bindValue(":LA2",loc_acc_2);
            addInclAttInVOIES.bindValue(":LA3",loc_acc_3);

            if (! addInclAttInVOIES.exec()) {
                pLogger->ERREUR(QString("Impossible d'inserer l'accessibilite locale 1 %1 2 %2 et 3 %3 pour la voie %4").arg(loc_acc_1).arg(loc_acc_2).arg(loc_acc_3).arg(idv1));
                pLogger->ERREUR(addInclAttInVOIES.lastError().text());
                return false;
            }

        }//end for idv1

        if (! pDatabase->add_att_cl("VOIES", "CL_LA1", "LOCAL_ACCESS1", 10, true)) return false;
        if (! pDatabase->add_att_cl("VOIES", "CL_LA2", "LOCAL_ACCESS2", 10, true)) return false;
        if (! pDatabase->add_att_cl("VOIES", "CL_LA3", "LOCAL_ACCESS3", 10, true)) return false;

    } else {
        pLogger->INFO("---------------- Local Access attributes already in VOIES -----------------");
    }

    return true;

}//END calcLocalAccess

bool Voies::calcBruitArcs(){

    if (! pDatabase->columnExists("SIF", "BRUIT_1") || ! pDatabase->columnExists("SIF", "BRUIT_2")  || ! pDatabase->columnExists("SIF", "BRUIT_3")  || ! pDatabase->columnExists("SIF", "BRUIT_4")) {
        pLogger->INFO("---------------------- calcBruitArcs START ----------------------");

        // AJOUT DE L'ATTRIBUT DE BRUIT
        QSqlQueryModel addBruitInSIF;
        addBruitInSIF.setQuery("ALTER TABLE SIF ADD BRUIT_1 float, ADD BRUIT_2 float, ADD BRUIT_3 float, ADD BRUIT_4 float;");

        if (addBruitInSIF.lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible d'ajouter les attributs de bruit dans SIF : %1").arg(addBruitInSIF.lastError().text()));
            return false;
        }

        QSqlQueryModel *attFromSIF = new QSqlQueryModel();

        attFromSIF->setQuery("SELECT IDA, SI, SF, IDV, ST_length(GEOM) as LENGTH FROM SIF;");

        if (attFromSIF->lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible de recuperer les attributs dans SIF: %1").arg(attFromSIF->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        int nbArcs = attFromSIF->rowCount();

        //CREATION DU TABLEAU DONNANT LES ATTRIBUTS DE CHAQUE ARC
        int idv_arc[nbArcs + 1];
        int si_arc[nbArcs + 1];
        int sf_arc[nbArcs + 1];
        float length_arc[nbArcs + 1];

        for(int i = 0; i < nbArcs + 1; i++){
            idv_arc[i] = 0;
            si_arc[i] = 0;
            sf_arc[i] = 0;
            length_arc[i] = 0;
        }

        for(int a = 0; a < nbArcs; a++){
            int ida = attFromSIF->record(a).value("IDA").toInt();
            idv_arc[ida]=attFromSIF->record(a).value("IDV").toInt();
            si_arc[ida]=attFromSIF->record(a).value("SI").toInt();
            sf_arc[ida]=attFromSIF->record(a).value("SF").toInt();
            length_arc[ida]=attFromSIF->record(a).value("LENGTH").toInt();
        }//end for v

        //SUPPRESSION DE L'OBJET
        delete attFromSIF;

        for(int ida1 = 1; ida1 < nbArcs + 1; ida1++){

            float bruit_1 = 0;
            float bruit_2 = 0;
            float bruit_3 = 0;
            float bruit_4 = 0;

            int arc_comb1 = 0;
            int arc_comb2 = 0;
            int arc_comb3 = 0;
            int arc_comb4 = 0;

            //on parcourt l'ensemble des arcs du tableau
            for (int ida2 = 1; ida2 < nbArcs + 1; ida2 ++) {

                if((ida1 != ida2 && ida2 != arc_comb1 && ida2 != arc_comb2 && ida2 != arc_comb3) //l'arc est différent de ceux qu'on a déjà
                && (idv_arc[ida1] == idv_arc[ida2]) // les deux arcs sont sur la même voie
                && (si_arc[ida1]==si_arc[ida2] || si_arc[ida1]==sf_arc[ida2] || sf_arc[ida1]==sf_arc[ida2] || sf_arc[ida1]==si_arc[ida2]) // les deux arcs partagent un sommet
                && length_arc[ida1] != 0
                && length_arc[ida2] != 0
                ){

                    pLogger->INFO(QString("*************Arc de base : %1").arg(ida1));

                        if(bruit_1 == 0){
                            bruit_1 = length_arc[ida1] / (length_arc[ida1] + length_arc[ida2]);
                            arc_comb1 = ida2;
                            pLogger->INFO(QString("Combine 1 : %1, bruit : %2").arg(arc_comb1).arg(bruit_1));
                        }
                        else if(bruit_2 == 0){
                            bruit_2 = length_arc[ida1] / (length_arc[ida1] + length_arc[ida2]);
                            arc_comb2 = ida2;
                            pLogger->INFO(QString("Combine 2 : %1, bruit : %2").arg(arc_comb2).arg(bruit_2));
                        }
                        else if(bruit_3 == 0){ // cas d'une boucle
                            bruit_3 = length_arc[ida1] / (length_arc[ida1] + length_arc[ida2]);
                            arc_comb3 = ida2;
                            pLogger->INFO(QString("Combine 3 : %1, bruit : %2").arg(arc_comb3).arg(bruit_3));

                        }
                        else if(bruit_4 == 0){ // cas d'une boucle
                            bruit_4 = length_arc[ida1] / (length_arc[ida1] + length_arc[ida2]);
                            arc_comb4 = ida2;
                            pLogger->INFO(QString("Combine 4 : %1, bruit : %2").arg(arc_comb4).arg(bruit_4));

                        }else{
                        pLogger->ERREUR(QString("ATTENTION CAS PARTICULIER : les quatre bruits sont remplis : B1 = %1 (arc %2) ; B2 = %3 (arc %4) ; B3 = %5 (arc %6) pour l'arc ida1 = %4").arg(bruit_1).arg(arc_comb1).arg(bruit_2).arg(arc_comb2).arg(bruit_3).arg(arc_comb3).arg(ida1));
                        //return false;
                        }
                }

            }//end for a2


            //INSERTION EN BASE
            QSqlQuery addBruitAttInSIF;
            addBruitAttInSIF.prepare("UPDATE SIF SET BRUIT_1 = :B1, BRUIT_2 = :B2, BRUIT_3 = :B3, BRUIT_4 = :B4 WHERE ida = :IDA ;");
            addBruitAttInSIF.bindValue(":IDA",ida1 );
            addBruitAttInSIF.bindValue(":B1",bruit_1);
            addBruitAttInSIF.bindValue(":B2",bruit_2);
            addBruitAttInSIF.bindValue(":B3",bruit_3);
            addBruitAttInSIF.bindValue(":B4",bruit_4);

            if (! addBruitAttInSIF.exec()) {
                pLogger->ERREUR(QString("Impossible d'inserer les bruits 1 %1, 2 %2 et 3 %3 pour l'arc' %4").arg(bruit_1).arg(bruit_2).arg(bruit_3).arg(ida1));
                pLogger->ERREUR(addBruitAttInSIF.lastError().text());
                return false;
            }

        }//end for ida1

    } else {
        pLogger->INFO("---------------- Bruit already in SIF -----------------");
    }

    return true;

}//END calcBruitArcs


bool Voies::calcGradient(){

    if (! pDatabase->columnExists("VOIES", "GRAD") || ! pDatabase->columnExists("VOIES", "GRAD_MOY")) {
        pLogger->INFO("---------------------- calcGradient START ----------------------");

        // AJOUT DE L'ATTRIBUT DE GRADIENT
        QSqlQueryModel addGradlInVOIES;
        addGradlInVOIES.setQuery("ALTER TABLE VOIES ADD GRAD float, ADD GRAD_MOY float;");

        if (addGradlInVOIES.lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible d'ajouter les attributs de gradient dans VOIES : %1").arg(addGradlInVOIES.lastError().text()));
            return false;
        }

        QSqlQueryModel *structFromVOIES = new QSqlQueryModel();

        structFromVOIES->setQuery("SELECT IDV, STRUCT AS STRUCT_VOIE FROM VOIES;");

        if (structFromVOIES->lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible de recuperer les structuralites dans VOIES pour calculer le gradient : %1").arg(structFromVOIES->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        //CREATION DU TABLEAU DONNANT LA STRUCTURALITE DE CHAQUE VOIE
        float struct_voie[m_nbVoies + 1];

        for(int i = 0; i < m_nbVoies + 1; i++){
            struct_voie[i] = 0;
        }

        for(int v = 0; v < m_nbVoies; v++){
            int idv = structFromVOIES->record(v).value("IDV").toInt();
            struct_voie[idv]=structFromVOIES->record(v).value("STRUCT_VOIE").toFloat();
            m_struct_tot += struct_voie[idv];
        }//end for v

        //SUPPRESSION DE L'OBJET
        delete structFromVOIES;

        //***********************************************Calcul du gradient :

        for(int idv0 = 1; idv0 < m_nbVoies + 1; idv0++){

            float gradient = 0;
            int size_idv3 = 0;
            float gradient_moy = 0;

            //on cherche toutes les voies connectées à idv0
            for (int v1 = 0; v1 < m_VoieVoies.at(idv0).size(); v1 ++) {
                long idv1 = m_VoieVoies.at(idv0).at(v1);

                gradient += fabs(struct_voie[idv0]-struct_voie[idv1]);
                size_idv3 += 1;

                //on cherche toutes les voies connectées à idv1
                for (int v2 = 0; v2 < m_VoieVoies.at(idv1).size(); v2 ++) {
                    long idv2 = m_VoieVoies.at(idv1).at(v2);

                    gradient += fabs(struct_voie[idv0]-struct_voie[idv2]);
                    size_idv3 += 1;

                    //on cherche toutes les voies connectées à idv2
                    for (int v3 = 0; v3 < m_VoieVoies.at(idv2).size(); v3 ++) {
                        long idv3 = m_VoieVoies.at(idv2).at(v3);


                        gradient += fabs(struct_voie[idv0]-struct_voie[idv3]);
                        size_idv3 += 1;


                    }//end for idv3


                }//end for idv2


            }//end for idv1


            if(size_idv3 != 0) {gradient_moy = gradient / size_idv3;}
            else {gradient_moy = 0;}

            //INSERTION EN BASE
            QSqlQuery addInclAttInVOIES;
            addInclAttInVOIES.prepare("UPDATE VOIES SET GRAD = :GRAD, GRAD_MOY = :GRAD_MOY WHERE idv = :IDV ;");
            addInclAttInVOIES.bindValue(":IDV",idv0 );
            addInclAttInVOIES.bindValue(":GRAD",gradient);
            addInclAttInVOIES.bindValue(":GRAD_MOY",gradient_moy);

            if (! addInclAttInVOIES.exec()) {
                pLogger->ERREUR(QString("Impossible d'inserer le gradient %1 et le gradient moyenne %2 pour la voie %3").arg(gradient).arg(gradient_moy).arg(idv0));
                pLogger->ERREUR(addInclAttInVOIES.lastError().text());
                return false;
            }

        }//end for idv0

        if (! pDatabase->add_att_cl("VOIES", "CL_GRAD", "GRAD", 10, true)) return false;
        if (! pDatabase->add_att_cl("VOIES", "CL_GRADMOY", "GRAD_MOY", 10, true)) return false;

        pLogger->INFO(QString("STRUCTURALITE TOTALE SUR LE RESEAU : %1").arg(m_struct_tot));

    } else {
        pLogger->INFO("---------------- Grad attributes already in VOIES -----------------");
    }

    return true;

}//END calcGradient







//***************************************************************************************************************************************************
//INSERT NV ATT DE LA TABLE INFO EN BDD
//
//***************************************************************************************************************************************************

bool Voies::insertINFO(){

    pLogger->INFO("-------------------------- start insertINFO --------------------------");

    //STOCKAGE DES INFOS*******

    if (! pDatabase->tableExists("INFO")){
        pLogger->ERREUR("La table INFO doit etre presente au moment des voies");
        return false;
    }//endif

    // Test si existe methode + seuil

    QSqlQueryModel alreadyInTable;
    alreadyInTable.setQuery(QString("SELECT * FROM INFO WHERE methode = %1 AND seuil_angle = %2").arg((int) m_methode).arg((int) m_seuil_angle));
    if (alreadyInTable.lastError().isValid()) {
        pLogger->ERREUR(QString("Ne peut pas tester l'existance dans table de cette methode et seuil : %1").arg(alreadyInTable.lastError().text()));
        return false;
    }

    bool isInTable = (alreadyInTable.rowCount() > 0);
    bool isActive = false;
    if (isInTable) isActive = alreadyInTable.record(0).value("active").toBool();

    if (isInTable && isActive) {
        pLogger->INFO(QString("Methode = %1 et seuil = %2 est deja dans la table INFO, et est actif").arg(m_methode).arg(m_seuil_angle));
        return true;
    } else if (isInTable && ! isActive) {
        QSqlQuery desactiveTous;
        desactiveTous.prepare("UPDATE INFO SET ACTIVE=FALSE;");

        if (! desactiveTous.exec()) {
            pLogger->ERREUR(QString("Impossible de desactiver toutes les configuratiosn de la table info : %1").arg(desactiveTous.lastError().text()));
            return false;
        }

        QSqlQuery activeUn;
        activeUn.prepare(QString("UPDATE INFO SET ACTIVE=TRUE WHERE methode = %1 AND seuil_angle = %2").arg((int) m_methode).arg((int) m_seuil_angle));

        if (! activeUn.exec()) {
            pLogger->ERREUR(QString("Impossible d'activer la bonne configuration toutes les configuration de la table info : %1").arg(activeUn.lastError().text()));
            return false;
        }

    } else {
        // La configuration n'est pas dans la table INFO
        QSqlQuery desactiveTous;
        desactiveTous.prepare("UPDATE INFO SET ACTIVE=FALSE;");

        if (! desactiveTous.exec()) {
            pLogger->ERREUR(QString("Impossible de desactiver toutes les configuration de la table info : %1").arg(desactiveTous.lastError().text()));
            return false;
        }

        QSqlQueryModel *req_voies_avg = new QSqlQueryModel();
        req_voies_avg->setQuery(  "SELECT "

                                  "AVG(nba) as AVG_NBA, "
                                  "AVG(nbs) as AVG_NBS, "
                                  "AVG(nbc) as AVG_NBC, "
                                  "AVG(rtopo) as AVG_O, "
                                  "AVG(struct) as AVG_S, "

                                  "STDDEV(length) as STD_L, "
                                  "STDDEV(nba) as STD_NBA, "
                                  "STDDEV(nbs) as STD_NBS, "
                                  "STDDEV(nbc) as STD_NBC, "
                                  "STDDEV(rtopo) as STD_O, "
                                  "STDDEV(struct) as STD_S, "

                                  "AVG(LOG(length)) as AVG_LOG_L, "
                                  "AVG(LOG(nba)) as AVG_LOG_NBA, "
                                  "AVG(LOG(nbs)) as AVG_LOG_NBS, "
                                  "AVG(LOG(nbc)) as AVG_LOG_NBC, "
                                  "AVG(LOG(rtopo)) as AVG_LOG_O, "
                                  "AVG(LOG(struct)) as AVG_LOG_S, "

                                  "STDDEV(LOG(length)) as STD_LOG_L, "
                                  "STDDEV(LOG(nba)) as STD_LOG_NBA, "
                                  "STDDEV(LOG(nbs)) as STD_LOG_NBS, "
                                  "STDDEV(LOG(nbc)) as STD_LOG_NBC, "
                                  "STDDEV(LOG(rtopo)) as STD_LOG_O, "
                                  "STDDEV(LOG(struct)) as STD_LOG_S "

                                  "FROM VOIES "

                                  "WHERE length > 0 AND nba > 0 AND nbs > 0 AND nbc > 0 AND rtopo > 0 AND struct > 0;");

        if (req_voies_avg->lastError().isValid()) {
            pLogger->ERREUR(QString("create_info - req_voies_avg : %1").arg(req_voies_avg->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        float avg_nba = req_voies_avg->record(0).value("AVG_NBA").toFloat();
        float avg_nbs = req_voies_avg->record(0).value("AVG_NBS").toFloat();
        float avg_nbc = req_voies_avg->record(0).value("AVG_NBC").toFloat();
        float avg_o = req_voies_avg->record(0).value("AVG_O").toFloat();
        float avg_s = req_voies_avg->record(0).value("AVG_S").toFloat();

        float std_length = req_voies_avg->record(0).value("STD_L").toFloat();
        float std_nba = req_voies_avg->record(0).value("STD_NBA").toFloat();
        float std_nbs = req_voies_avg->record(0).value("STD_NBS").toFloat();
        float std_nbc = req_voies_avg->record(0).value("STD_NBC").toFloat();
        float std_o = req_voies_avg->record(0).value("STD_O").toFloat();
        float std_s = req_voies_avg->record(0).value("STD_S").toFloat();

        float avg_log_length = req_voies_avg->record(0).value("AVG_LOG_L").toFloat();
        float avg_log_nba = req_voies_avg->record(0).value("AVG_LOG_NBA").toFloat();
        float avg_log_nbs = req_voies_avg->record(0).value("AVG_LOG_NBS").toFloat();
        float avg_log_nbc = req_voies_avg->record(0).value("AVG_LOG_NBC").toFloat();
        float avg_log_o = req_voies_avg->record(0).value("AVG_LOG_O").toFloat();
        float avg_log_s = req_voies_avg->record(0).value("AVG_LOG_S").toFloat();

        float std_log_length = req_voies_avg->record(0).value("STD_LOG_L").toFloat();
        float std_log_nba = req_voies_avg->record(0).value("STD_LOG_NBA").toFloat();
        float std_log_nbs = req_voies_avg->record(0).value("STD_LOG_NBS").toFloat();
        float std_log_nbc = req_voies_avg->record(0).value("STD_LOG_NBC").toFloat();
        float std_log_o = req_voies_avg->record(0).value("STD_LOG_O").toFloat();
        float std_log_s = req_voies_avg->record(0).value("STD_LOG_S").toFloat();

        //SUPPRESSION DE L'OBJET
        delete req_voies_avg;

        QSqlQueryModel *req_angles_avg = new QSqlQueryModel();
        req_angles_avg->setQuery( "SELECT "
                                  "AVG(angle) as AVG_ANG, "
                                  "STDDEV(angle) as STD_ANG "

                                  "FROM ANGLES WHERE USED;");

        if (req_angles_avg->lastError().isValid()) {
            pLogger->ERREUR(QString("create_info - req_angles_avg : %1").arg(req_angles_avg->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        float avg_ang = req_angles_avg->record(0).value("AVG_ANG").toFloat();
        float std_ang = req_angles_avg->record(0).value("STD_ANG").toFloat();

        //SUPPRESSION DE L'OBJET
        delete req_angles_avg;

        pLogger->INFO(QString("m_length_tot : %1").arg(m_length_tot));

        QSqlQuery info_in_db;
        info_in_db.prepare("INSERT INTO INFO ("

                           "methode, seuil_angle, LTOT, N_COUPLES, N_CELIBATAIRES, NV, "
                           "AVG_LVOIE, STD_LVOIE, AVG_ANG, STD_ANG, AVG_NBA, STD_NBA, AVG_NBS, STD_NBS, AVG_NBC, STD_NBC, AVG_O, STD_O, AVG_S, STD_S, "
                           "AVG_LOG_LVOIE, STD_LOG_LVOIE, AVG_LOG_NBA, STD_LOG_NBA, AVG_LOG_NBS, STD_LOG_NBS, AVG_LOG_NBC, STD_LOG_NBC, AVG_LOG_O, STD_LOG_O, AVG_LOG_S, STD_LOG_S, "
                           "ACTIVE) "

                           "VALUES ("

                           ":M, :SA, :LTOT, :N_COUPLES, :N_CELIBATAIRES, :NV, "
                           ":AVG_LVOIE, :STD_LVOIE, :AVG_ANG, :STD_ANG, :AVG_NBA, :STD_NBA, :AVG_NBS, :STD_NBS, :AVG_NBC, :STD_NBC, :AVG_O, :STD_O, :AVG_S, :STD_S, "
                           ":AVG_LOG_LVOIE, :STD_LOG_LVOIE, :AVG_LOG_NBA, :STD_LOG_NBA, :AVG_LOG_NBS, :STD_LOG_NBS, :AVG_LOG_NBC, :STD_LOG_NBC, :AVG_LOG_O, :STD_LOG_O, :AVG_LOG_S, :STD_LOG_S, "
                           "TRUE);");

        info_in_db.bindValue(":M",m_methode);
        info_in_db.bindValue(":SA",m_seuil_angle);
        info_in_db.bindValue(":LTOT",QVariant(m_length_tot));
        info_in_db.bindValue(":N_COUPLES",QVariant(m_nbCouples));
        info_in_db.bindValue(":N_CELIBATAIRES",QVariant(m_nbCelibataire));
        info_in_db.bindValue(":NV",QVariant(m_nbVoies));

        info_in_db.bindValue(":AVG_LVOIE",QVariant(m_length_tot/m_nbVoies));

        info_in_db.bindValue(":AVG_ANG",avg_ang);
        info_in_db.bindValue(":STD_ANG",std_ang);

        info_in_db.bindValue(":AVG_NBA",avg_nba);
        info_in_db.bindValue(":AVG_NBS",avg_nbs);
        info_in_db.bindValue(":AVG_NBC",avg_nbc);
        info_in_db.bindValue(":AVG_O",avg_o);
        info_in_db.bindValue(":AVG_S",avg_s);

        info_in_db.bindValue(":STD_LVOIE",std_length);
        info_in_db.bindValue(":STD_NBA",std_nba);
        info_in_db.bindValue(":STD_NBA",std_nba);
        info_in_db.bindValue(":STD_NBS",std_nbs);
        info_in_db.bindValue(":STD_NBC",std_nbc);
        info_in_db.bindValue(":STD_O",std_o);
        info_in_db.bindValue(":STD_S",std_s);

        info_in_db.bindValue(":AVG_LOG_LVOIE",avg_log_length);
        info_in_db.bindValue(":AVG_LOG_NBA",avg_log_nba);
        info_in_db.bindValue(":AVG_LOG_NBS",avg_log_nbs);
        info_in_db.bindValue(":AVG_LOG_NBC",avg_log_nbc);
        info_in_db.bindValue(":AVG_LOG_O",avg_log_o);
        info_in_db.bindValue(":AVG_LOG_S",avg_log_s);

        info_in_db.bindValue(":STD_LOG_LVOIE",std_log_length);
        info_in_db.bindValue(":STD_LOG_NBA",std_log_nba);
        info_in_db.bindValue(":STD_LOG_NBA",std_log_nba);
        info_in_db.bindValue(":STD_LOG_NBS",std_log_nbs);
        info_in_db.bindValue(":STD_LOG_NBC",std_log_nbc);
        info_in_db.bindValue(":STD_LOG_O",std_log_o);
        info_in_db.bindValue(":STD_LOG_S",std_log_s);


        if (! info_in_db.exec()) {
            pLogger->ERREUR(QString("Impossible d'inserer les infos (sur VOIES) dans la table INFO : %1").arg(info_in_db.lastError().text()));
            return false;
        }

    }

    pLogger->INFO("--------------------------- end insertINFO ---------------------------");

    return true;

}//END insertINFO

//***************************************************************************************************************************************************

//***************************************************************************************************************************************************
//CONSTRUCTION DE LA TABLE DES VOIES
//
//***************************************************************************************************************************************************

bool Voies::do_Voies(){

    // Construction des couples d'arcs
    if (! buildCouples()) return false;

    // Construction des attributs membres des voies
    if (! buildVectors()) return false;

    // Construction de la table VOIES en BDD
    if (! build_VOIES()) {
        if (! pDatabase->dropTable("VOIES")) {
            pLogger->ERREUR("build_VOIES en erreur, ROLLBACK (drop VOIES) echoue");
        } else {
            pLogger->INFO("build_VOIES en erreur, ROLLBACK (drop VOIES) reussi");
        }
        return false;
    }

    return true;

}//end do_Voie

//***************************************************************************************************************************************************
//INSERTION DES ATTRIBUTS DES VOIES
//
//***************************************************************************************************************************************************

bool Voies::do_Att_Arc(){

        if (! calcBruitArcs()) {
            if (! pDatabase->dropColumn("SIF", "BRUIT_1")) {
                pLogger->ERREUR("calcBruitArcs en erreur, ROLLBACK (drop column BRUIT_1) echoue");
            } else {
                pLogger->INFO("calcBruitArcs en erreur, ROLLBACK (drop column BRUIT_1) reussi");
            }
            if (! pDatabase->dropColumn("SIF", "BRUIT_2")) {
                pLogger->ERREUR("calcBruitArcs en erreur, ROLLBACK (drop column BRUIT_2) echoue");
            } else {
                pLogger->INFO("calcBruitArcs en erreur, ROLLBACK (drop column BRUIT_2) reussi");
            }
            if (! pDatabase->dropColumn("SIF", "BRUIT_3")) {
                pLogger->ERREUR("calcBruitArcs en erreur, ROLLBACK (drop column BRUIT_3) echoue");
            } else {
                pLogger->INFO("calcBruitArcs en erreur, ROLLBACK (drop column BRUIT_3) reussi");
            }
            if (! pDatabase->dropColumn("SIF", "BRUIT_4")) {
                pLogger->ERREUR("calcBruitArcs en erreur, ROLLBACK (drop column BRUIT_3) echoue");
            } else {
                pLogger->INFO("calcBruitArcs en erreur, ROLLBACK (drop column BRUIT_3) reussi");
            }
            return false;
        }

}//end do_Att_Arc

bool Voies::do_Att_Voie(bool connexion, bool use, bool inclusion, bool gradient, bool local_access){

    // Calcul de la structuralite
    if (! calcStructuralite()) {
        if (! pDatabase->dropColumn("VOIES", "DEGREE")) {
            pLogger->ERREUR("calcStructuralite en erreur, ROLLBACK (drop column DEGREE) echoue");
        } else {
            pLogger->INFO("calcStructuralite en erreur, ROLLBACK (drop column DEGREE) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "RTOPO")) {
            pLogger->ERREUR("calcStructuralite en erreur, ROLLBACK (drop column RTOPO) echoue");
        } else {
            pLogger->INFO("calcStructuralite en erreur, ROLLBACK (drop column RTOPO) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "STRUCT")) {
            pLogger->ERREUR("calcStructuralite en erreur, ROLLBACK (drop column STRUCT) echoue");
        } else {
            pLogger->INFO("calcStructuralite en erreur, ROLLBACK (drop column STRUCT) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "CL_S")) {
            pLogger->ERREUR("calcStructuralite en erreur, ROLLBACK (drop column CL_S) echoue");
        } else {
            pLogger->INFO("calcStructuralite en erreur, ROLLBACK (drop column CL_S) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "SOL")) {
            pLogger->ERREUR("calcStructuralite en erreur, ROLLBACK (drop column SOL) echoue");
        } else {
            pLogger->INFO("calcStructuralite en erreur, ROLLBACK (drop column SOL) reussi");
        }
        return false;
    }

    if (! calcStructRel()){
        if (! pDatabase->dropColumn("VOIES", "RTOPO_SCL0")) {
            pLogger->ERREUR("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_SCL0) echoue");
        } else {
            pLogger->INFO("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_SCL0) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "RTOPO_SCL1")) {
            pLogger->ERREUR("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_SCL1) echoue");
        } else {
            pLogger->INFO("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_SCL1) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "RTOPO_MCL0")) {
            pLogger->ERREUR("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_MCL0) echoue");
        } else {
            pLogger->INFO("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_MCL0) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "RTOPO_MCL1")) {
            pLogger->ERREUR("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_MCL1) echoue");
        } else {
            pLogger->INFO("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_MCL1) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "RTOPO_S1")) {
            pLogger->ERREUR("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_S1) echoue");
        } else {
            pLogger->INFO("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_S1) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "RTOPO_S2")) {
            pLogger->ERREUR("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_S2) echoue");
        } else {
            pLogger->INFO("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_S2) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "RTOPO_S3")) {
            pLogger->ERREUR("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_S3) echoue");
        } else {
            pLogger->INFO("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_S3) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "RTOPO_S4")) {
            pLogger->ERREUR("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_S4) echoue");
        } else {
            pLogger->INFO("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_S4) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "RTOPO_S5")) {
            pLogger->ERREUR("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_S5) echoue");
        } else {
            pLogger->INFO("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_S5) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "RTOPO_S6")) {
            pLogger->ERREUR("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_S6) echoue");
        } else {
            pLogger->INFO("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_S6) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "RTOPO_S7")) {
            pLogger->ERREUR("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_S7) echoue");
        } else {
            pLogger->INFO("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_S7) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "RTOPO_S8")) {
            pLogger->ERREUR("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_S8) echoue");
        } else {
            pLogger->INFO("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_S8) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "RTOPO_S9")) {
            pLogger->ERREUR("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_S9) echoue");
        } else {
            pLogger->INFO("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_S9) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "RTOPO_S10")) {
            pLogger->ERREUR("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_S10) echoue");
        } else {
            pLogger->INFO("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_S10) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "RTOPO_M1")) {
            pLogger->ERREUR("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_M1) echoue");
        } else {
            pLogger->INFO("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_M1) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "RTOPO_M2")) {
            pLogger->ERREUR("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_M2) echoue");
        } else {
            pLogger->INFO("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_M2) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "RTOPO_M3")) {
            pLogger->ERREUR("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_M31) echoue");
        } else {
            pLogger->INFO("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_M3) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "RTOPO_M4")) {
            pLogger->ERREUR("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_M4) echoue");
        } else {
            pLogger->INFO("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_M4) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "RTOPO_M5")) {
            pLogger->ERREUR("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_M5) echoue");
        } else {
            pLogger->INFO("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_M5) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "RTOPO_M6")) {
            pLogger->ERREUR("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_M6) echoue");
        } else {
            pLogger->INFO("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_M6) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "RTOPO_M7")) {
            pLogger->ERREUR("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_M7) echoue");
        } else {
            pLogger->INFO("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_M7) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "RTOPO_M8")) {
            pLogger->ERREUR("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_M8) echoue");
        } else {
            pLogger->INFO("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_M8) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "RTOPO_M9")) {
            pLogger->ERREUR("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_M9) echoue");
        } else {
            pLogger->INFO("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_M9) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "RTOPO_M10")) {
            pLogger->ERREUR("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_M10) echoue");
        } else {
            pLogger->INFO("calcStructuraliteRel en erreur, ROLLBACK (drop column RTOPO_M10) reussi");
        }
        return false;
    }

    //calcul des angles de connexions (orthogonalité)
    if (connexion && ! calcConnexion()) {
        if (! pDatabase->dropColumn("VOIES", "NBCSIN")) {
            pLogger->ERREUR("calcConnexion en erreur, ROLLBACK (drop column NBCSIN) echoue");
        } else {
            pLogger->INFO("calcConnexion en erreur, ROLLBACK (drop column NBCSIN) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "NBCSIN_P")) {
            pLogger->ERREUR("calcConnexion en erreur, ROLLBACK (drop column NBCSIN_P) echoue");
        } else {
            pLogger->INFO("calcConnexion en erreur, ROLLBACK (drop column NBCSIN_P) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "C_ORTHO")) {
            pLogger->ERREUR("calcConnexion en erreur, ROLLBACK (drop column C_ORTHO) echoue");
        } else {
            pLogger->INFO("calcConnexion en erreur, ROLLBACK (drop column C_ORTHO) reussi");
        }
        return false;

    }

    //calcul des utilisation (betweenness)
    if (use && ! calcUse()) {
        if (! pDatabase->dropColumn("VOIES", "USE")) {
            pLogger->ERREUR("calcUse en erreur, ROLLBACK (drop column USE) echoue");
        } else {
            pLogger->INFO("calcUse en erreur, ROLLBACK (drop column USE) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "USE_MLT")) {
            pLogger->ERREUR("calcUse en erreur, ROLLBACK (drop column USE_MLT) echoue");
        } else {
            pLogger->INFO("calcUse en erreur, ROLLBACK (drop column USE_MLT) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "USE_LGT")) {
            pLogger->ERREUR("calcUse en erreur, ROLLBACK (drop column USE_LGT) echoue");
        } else {
            pLogger->INFO("calcUse en erreur, ROLLBACK (drop column USE_LGT) reussi");
        }
        return false;
    }

    // Calcul de l'inclusion
    if (inclusion && ! calcInclusion()) {
        if (! pDatabase->dropColumn("VOIES", "INCL")) {
            pLogger->ERREUR("calcInclusion en erreur, ROLLBACK (drop column INCL) echoue");
        } else {
            pLogger->INFO("calcInclusion en erreur, ROLLBACK (drop column INCL) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "INCL_MOY")) {
            pLogger->ERREUR("calcInclusion en erreur, ROLLBACK (drop column INCL_MOY) echoue");
        } else {
            pLogger->INFO("calcInclusion en erreur, ROLLBACK (drop column INCL_MOY) reussi");
        }
        return false;
    }

    // Calcul de l'accebilité locale
    if(local_access && ! calcLocalAccess() ){
        if (! pDatabase->dropColumn("VOIES", "LOCAL_ACCESS1")) {
            pLogger->ERREUR("calcLocalAccess en erreur, ROLLBACK (drop column LOCAL_ACCESS1) echoue");
        } else {
            pLogger->INFO("calcLocalAccess en erreur, ROLLBACK (drop column LOCAL_ACCESS1) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "LOCAL_ACCESS2")) {
            pLogger->ERREUR("calcLocalAccess en erreur, ROLLBACK (drop column LOCAL_ACCESS2) echoue");
        } else {
            pLogger->INFO("calcLocalAccess en erreur, ROLLBACK (drop column LOCAL_ACCESS2) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "LOCAL_ACCESS3")) {
            pLogger->ERREUR("calcLocalAccess en erreur, ROLLBACK (drop column LOCAL_ACCESS3) echoue");
        } else {
            pLogger->INFO("calcLocalAccess en erreur, ROLLBACK (drop column LOCAL_ACCESS3) reussi");
        }
        return false;
    }

    // Calcul du gradient
    if (gradient && ! calcGradient()) {
        if (! pDatabase->dropColumn("VOIES", "GRAD")) {
            pLogger->ERREUR("calcGradient en erreur, ROLLBACK (drop column GRAD) echoue");
        } else {
            pLogger->INFO("calcGradient en erreur, ROLLBACK (drop column GRAD) reussi");
        }
        if (! pDatabase->dropColumn("VOIES", "GRAD_MOY")) {
            pLogger->ERREUR("calcGradient en erreur, ROLLBACK (drop column GRAD_MOY) echoue");
        } else {
            pLogger->INFO("calcGradient en erreur, ROLLBACK (drop column GRAD_MOY) reussi");
        }
        return false;
    }


    //construction de la table VOIES en BDD
    if (! insertINFO()) return false;

    //upadte SIF
    if (! updateSIF()) return false;

    return true;

}//end do_Att_Voie
