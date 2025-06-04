- Commencer les commandes operators + commandes de base
  PING (a completer)
  JOIN : <- Juste a ignorer jcrois
  CAP END ?
  PART
- Faire le parsing pour les commandes plutot que de faire 5 lignes pour chaque commande a chaque fois (Puis clean la grosse fonction handleClientMessage)
- Trouver si on peut faire la diff entre client irssi et nc (pour pas afficher sion)
- Verif si Nick deja utilise et si caractere valide (cf doc NICK)
- Envoyer bon message d'erreur
- Supprimer channel si plus personne dedans (Dans KICK et PART)
- On peux se kick soit meme. On garde ? On enleve ?