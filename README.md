# ft_irc

## 🧩 Description

**ft_irc** est une implémentation en C++ d’un serveur IRC conforme à une partie du protocole RFC 1459. Ce projet a été réalisé dans le cadre du cursus de l’école **42**.

Il permet la gestion de multiples clients via une socket TCP, le traitement de commandes IRC classiques, et la gestion de salons de discussion. Le serveur est conçu pour être compatible avec des clients IRC standards tels que **irssi**.

---

## ⚙️ Fonctionnalités principales

- Connexion de plusieurs clients simultanés via des sockets TCP
- Authentification via mot de passe (option `--pass`)
- Gestion de **channels** (création, jointure, départ, suppression)
- Commandes IRC classiques :
  - `/nick`, `/join`, `/part`, `/quit`, `/privmsg`, `/topic`, `/kick`, etc.
- Gestion des utilisateurs opérateurs
- Envoi de messages privés
- Compatibilité testée avec **irssi**

---

## 🛠️ Stack technique

- **Langage :** C++  
- **Programmation réseau :** sockets TCP  
- **Gestion des connexions :** `poll()`  
- **Parsing :** protocole texte IRC (RFC 1459 partiel)  
- **Système :** UNIX/Linux

---

## 🚀 Installation & Lancement

### 1. Cloner le dépôt

```bash
git clone https://github.com/celinelouaintier/irc.git
cd irc
```

### 2. Compiler

```bash
make
```

### 3. Lancer le serveur

```
./ircserv <port> <password>
```

Exemple :

```bash
./ircserv 6667 mysecurepass
```

### 4. Se connecter avec irssi

```bash
irssi
```

Dans le client :

```bash
/connect localhost 6667 mysecurepass
```

Puis :

```bash
/join #general
```

---

## 👥 Contributeurs

- [Céline Louaintier](https://github.com/celinelouaintier)
- [Xiaoyun Xu](https://github.com/Roychrltt)
- [Naïm Ferrad](https://github.com/Nyn9)

---

## 📝 Remarques

- Le projet respecte certaines limitations du sujet ft_irc de l’école 42.
- Il est volontairement simplifié par rapport à un serveur IRC complet, mais fournit une base fonctionnelle solide pour comprendre les communications réseau, la gestion de multiples clients et le parsing d’un protocole texte.

