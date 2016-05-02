# RainFbx - Plugin Freebox API pour Rainmeter

RainFbx est un plugin pour [Rainmeter](https://www.rainmeter.net/), utilisant des mesures récupérées au travers de l'[API de la Freebox](http://dev.freebox.fr/sdk/os/) du fournisseur d'accès [Free](http://www.free.fr).

## Installation

Téléchargez la [dernière release](https://github.com/Kitof/RainFbx/releases) correspondante à votre version de Rainmeter (32 ou 64 bits), et copiez le fichier dll dans le repertoire "Plugins\" de votre installation Rainmeter (par défaut: C:\Program Files\Rainmeter\Plugins).

## Autorisation

Lors du premier lancement, il est conseillé d'utiliser le skin _"illustro - Freebox Status & Bandwidth"_ pour faciliter l'étape d'autorisation.

Le plugin nécessite d'être autorisé via la facade de la Freebox lors du premier lancement.
L'utilisation des mesures RRD nécessite également l'ajout des permissions spécifiques au travers de l'interface FreeboxOS via _"Paramètres de la Freebox > Gestion des accès > Applications > Editer > Modifications des réglages de la Freebox"_

## Configuration avancée

Pour récupérer d'autres mesures de la Freebox et/ou pour intégrer d'autres skins, vous pouvez vous inspirez des skins fournis en exemple, ou lire le détails des variables disponibles ci-dessous.

La configuration du plugin nécessite au moins 2 sections au sein de votre fichier de configuration :

### Section mère "Configuration de la Freebox"

La section "Configuration de la Freebox" est généralement unique au sein de la configuration du skin (sauf à vouloir intégrer les mesures de plusieurs Freebox au sein d'un même skin).

Exemple :
```
[FreeboxConfig]
Measure=Plugin
Plugin=RainFbx.dll
Hostname="http://mafreebox.freebox.fr/"
AppToken=grEGErgeERG09ergRGE9greGREgkgrelgremgErgergEgezkafmlekvxemlsvkZl
```
#### Détails des variables ####

#####Measure / Plugin#####
_Obligatoires_

Les variables *Measure* et *Plugin* sont obligatoires, et doivent être fixées aux valeurs exemple pour indiquer quel plugin utiliser.

#####Hostname#####
_Optionnelle, valeur par défault = "http://mafreebox.freebox.fr/"_

Utilisez cette variable pour indiquer une url vers l'API de votre freebox différente de l'URL locale par défaut (pour utiliser une API distante par exemple).

#####AppToken#####
_Optionnelle, pas de valeur par défaut, mais renseignée automatiquement._

Cette variable sera ajoutée et renseignée automatiquement lors de la première autorisation du plugin auprès de la Freebox.
Renseignez là préalablement si vous la connaissez (lors de la duplication d'un skin par exemple).

### Section(s) fille(s) : "Configuration de la Mesure"

Il peut exister plusieurs sections "Configuration de Mesure" au sein d'un même fichier de configuration de skin.

Exemple :
```
[FbxMesureConfig]
Measure=Plugin
Plugin=RainFbx.dll
FbxAPIConf=freeboxConfig
MeasureDb=composite
MeasureField=busy_up
```
#### Détails des variables ####

#####Measure / Plugin#####
_Obligatoires_

Les variables *Measure* et *Plugin* sont obligatoires, et doivent être fixées aux valeurs exemple pour indiquer quel plugin utiliser.

#####FbxAPIConf#####
_Obligatoire, pas de valeur par défaut_

Cette variable doit indiquer vers quelle section "Configuration de la Freebox" cette section dépend.

#####MeasureDb#####
_Obligatoire, pas de valeur par défaut_

Cette variable indique au sein de quelle _database_ la mesure appartient. La liste des _databases_ est disponible sur le site de [Freebox API](http://dev.freebox.fr/sdk/os/rrd/).

La valeur _composite_ a été ajoutée au sein de ce plugin pour regrouper les mesures personalisées.

#####MeasureField#####
_Obligatoire, pas de valeur par défaut_

Cette variable indique le nom de la _mesure_. La liste des _mesures_ est disponible sur le site de [Freebox API](http://dev.freebox.fr/sdk/os/rrd/).

Les mesures suivantes personnalisées ont été ajoutées à la _database "composite"_ :
- **status** : Renvoie (via GetString) le status courant du plugin.
- **busy_down** : Renvoie le pourcentage d'occupation de la bande passante montante (download)
- **busy_up** : Renvoie le pourcentage d'occupation de la bande passante descendante (upload)

## Compilation ##

Le projet se compile en l'état sous Visual Studio 2015. Seule la librairie _C++ REST SDK_ doit être compilée statiquement (car trop lourde) à partir des sources en suivante le guide [How to statically link the C++ REST SDK (Casablanca)](https://katyscode.wordpress.com/2014/04/01/how-to-statically-link-the-c-rest-sdk-casablanca/) et placée dans le repertoire _lib/_ correspondant.
