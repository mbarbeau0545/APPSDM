# Introduction
    APP_SDM (System Diagnostic Management) est un module de diagnostic logiciel, 
    il doit être capable de répondre à différent besoin la surveillance,
    gestion des erreurs et leur journalisation. il doit également être capable d'informer 
    l'utilisateur d'une erreur selon sa sévérité à l'utilisateur via un moyen de comm (SERIAL, CAN, TCP/UDP etc)

# Configuration des errors
    l'utilisateur devra repertorier toutes les errors possibles de son application sous forme d'enum 
    l'utilisateur devra fournir pour chaque errors une configuration d'erreur sous forme de structure qui contiendra.
        - un debouncer delais en ms avant l'intervation direct de la logique de gestion de diagnostic.
        - un booleen pour dire si l'erreur doit être journalisé ou pas (occurences ).
        - Quelle SysFunction sont à inhiber une fois le debounquer passé.

# Information à garder pour le module 
    - un nombre maximum d'erreur qui peuvent être géré simultanément par le système.
    - status d'activation pour l'errreur.


# Utilisation du module 
    - l'utilisateur pourra émettre l'état d'une erreur via un API, en partageant des infos de debugages
    - l'utilisateur pourra savoir l'état d'une erreur via une API, 
    - l'utilisateur sera appelé lorsqu'une error aura atteint sa debounquer value et qu'il a renseigné qu'il veut être notifié, il sera appelé dans tout les cas si une ihnibition est faite quelque part dans le sysfunction.
    - Si une error est reshorber, n'est plus d'actualité, on doit pouvoir reset les valeurs qui font que le programme se bloque. 

# Gestion d'une erreur
    Lorqu'une erreur est repertorié elle peut avoir différent état -> 
        - soit l'erreur vient d'être détecté et il y a debouncer donc c'est l'état DEBOUNCER
        - soit le deboucer est passé et elle est toujours actif donc elle passe dans l'état ON
        - dans l'état ON on émet un message toutes les x secondes pour dire que l'erreur est toujours active 
            et on laisse set l'inhibition tant qu'on a pas d'aknowledment.

    Lorsque quelqu'un appelle le module pour repertorier une Erreur, deux cas possibles, 
    l'erreur n'est pas encore enregistré :
        -   On regarde la configuration lié à cette error,
            si il n'y a pas de debouncer, on passe l'error à l'état ON
            s'il y un debouncer, on sauvegarde le temps actuel, on set un flag globale pour dire 
            qu'on demande un suivi d'error pour cette error dans la cyclic.

    Gestionnaire d'erreur, on regarde la valeur du deboncer par rapport au temps actuel, si elle est dépassé
        on passe dans l'état ON.

    Dans l'état ON, tant qu'on a pas reçu de PASS on contacte l'API utilisateur toutes les x secondes avec les info de debug et l'enum, ce sera dans cette fonction que l'utilisateur sera libre de transmettre le diag via le protocol de son choix. Si l'état de l'errreur pas à OFF, on appelle une dernière fois le user avec l'état passé à PASS

    L'utilisateur devra donc fournir une callback s'il veut transmettre un message, l'idx de l'enum et les debug Info lui seront partagé. 

    On peut gérer le

# inhibition des agents 
    - La logic sera certainement architecturé en agent (voir Info APP_LGC) pour plus d'info, 
    donc après chaque debouncer si une startégy safety doit être appliqué, on set les agents dans un état d'inhibition totale (court-circuité totalement) ou dans un état de safety (a le droit de set actuators si un évènement compromettant la santé du robot survient) selon la configuration utilisateur.

# ReportDiagnosticEvnt
    Cette fonction est utilisé par l'utilisé pour report un diagnostic 
    Si il n'y a pas de diagnostic en cours de mngmt et l'evnt est PASS alors on sort de
    la fonction avec un retcode NO_OPERATION 
    l'exécution du reste de la fonction a donc deux buts 
        - soit le diagnostic existe et on met à jours l'evnt
        - soit il n'existe pas et on doit lui trouver une place grâce à 
            une variable d'index.