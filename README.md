# Aplicatie client-server TCP/UDP
## Chirimeaca Andrei 322CA

## Introducere
In cadrul acestui proiect, am avut de implementat o aplicatie in care exista mai multi clienti, acestia putand sa se aboneze la anumite topic-uri, iar in momentul in care este scris intr-un mesaj la un topic (de catre un utilizator UDP), acesta trebuie trimis mai departe la clientii online care sunt abonati la topicul respectiv.

## Implementare
**Server-ul** este cel care realizeaza toate aceste "redirectionari de mesaje", acest lucru fiind implementat cu ajutorul multiplexarii I/O. Conectarea utilizatorilor este realizata prin TCP, fiind utilizat un socket pe post de listen, iar in momentul in care un client se conecteaza, adaugam file descriptorul sau in lista de conexiuni a serverului, pentru a putea primi alte mesaje de la utilizatorul respectiv.

**Mesajele de la client**, de tip subscribe/unsubscribe <TOPIC> sunt, de asemenea, primite prin TCP, folosind un socket deschis in momentul conectarii clientului la server. Aceste mesaje sunt stocate intr-o strucutra numita tcp_message_from_server, iar in momentul in care acest pachet este primit de catre server, updatam hashmap-ul in care retinem clientii si topic-urile la care clientul respectiv este abonat.

**Mesajele de tip UDP** trebuie interpretate de server, acesta trebuind sa trimita respectivul mesaj doar la clientii care sunt abonati la topicul la care a fost scris mesajul. De asemenea, intrucat dimensiunea mesajul este una variabila, pentru a nu trimite "informatie goala", server-ul va realiza doua send-uri: unul care contine informatia legata de mesajul in sine(ip, port, topic-ul la care s-a postat, tipul informatiei din mesaj), iar al doilea va fi continutul mesajului in sine, fiind necesare doua send-uri pentru a se sti dimensiunea continutului in prealabil. Dupa ce server-ul primeste mesajul UDP si il "transforma" intr-unul de tip TCP, acesta il trimite doar la clientii abonati la topicul la care s-a postat mesajul UDP, folosind-use de hashmap-ul care stocheaza clientii si topicorile la care sunt abonati. In momentul in care clientul primeste un astfel de mesaj de la server, acesta trebuie sa interpreteze acest mesaj, in functie de tipul continutului care este in mesaj(INT, SHORT_REAL, FLOAT, STRING), asa cum este precizat in enunt.

**Mesaje de la STDIN**: pentru a putea citi comenzi de la tastatura, avem nevoie sa adaugam file descriptorul specific STDIN (STDIN_FILENO sau 0). In cazul serverului, avem doar comanda exit, care inchide serverul, dar pentru clienti putem avea si subscribe/unsubscribe, caz in care ne "construim" un pachet TCP, de data asta folosind alta structura, si anume tcp_message, care este una mai mica, deoarece contine mai putine informatii
