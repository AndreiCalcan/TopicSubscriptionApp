# Scurta descriere
Aceasta tema implementeaza servicii de abonare si dezabonare printr-o aplicatie client-server peste socketi TCP,
conform https://gitlab.cs.pub.ro/pcom/homework2-public/-/blob/main/Enunt_Tema_2_Protocoale_2022_2023.pdf.
Implementarea porneste de la scheletul laboratorului de multiplexare, dar este puternic modificat pentru cerintele curente.
Astfel, orice send/recv efectuat pe o conexiune TCP este facuta intr-o bucla pana la trimiterea intregului pachet.

# Detalii de implementare 
Serverul este rulat prin comanda ./server <port>. La rulare serverul dezactiveaza bufferingul la afisare si verifica
corectitudinea numarului de parametrii primiti. In cazul in care primeste prea putini/multi parametrii programul afiseaza
un mesaj sugestiv cu utilizarea corecta si se inchide. Pentru o rulare corecta serverul obtine de la sistemul de operare
doi socketi, unul pentru ascultarea de conexiuni TCP si unul pentru UDP, ale caror adrese sunt facute reutilizabile, iar 
pe TCP este dezactivat algoritmul lui Nagle. Acesti doi socketi sunt binded apoi pe toate interfetele locale si se incepe
rularea efectiva a serverului. Pentru setup serverul isi initializeaza structurile interne folosite pentru descriptorii
multiplxati (pollfds), lista a tuturor clientilor conectati (clients) si lista a tuturor topicelor de care serverul a auzit
de la pornire. Aceste structuri de date au la baza implementarea unui vector generic realizat in fisierul vector.c.
Serverul este setat apoi sa asculte pe socketul TCP cu dimensiunea maxima de backlog. Inainte de intrarea in functiune 
a serverului acesta isi adauga in vectorul de pollfds intrarile initiale (socketul de listen TCP, cel UDP si stdinul).
Intreaga logica de rulare a serverului are la baza o bucla infinita prin care se apeleaza functia de multiplexare poll
pe toate intrarile din pollfds. Pentru ca serverul sa nu ruleaze la infinit, iar oprirea lui sa fie graceful, la primirea
unei comenzi de exit de la tastatura acesta elibereaza toata memoria folosita si inchide socketii inainte de terminarea
executiei. Cand serverul primeste un connect de la un client TCP, acesta ii da accept, il adauga in vectorul de pollfds
si asteapta primirea unui ID. In functie de IDul primit serverul distinge 3 comportamente diferite. Primul caz este acela 
cand exista deja conectat un client cu acelasi ID, caz in care se afiseaza la stdout un mesaj sugestiv si se inchide
imediat socketul nou creat. Al doilea caz este acela cand IDul se afla in memoria serverului, atunci serverul efectueaza
o cautare in memorie prin toate topicele pentru a gasi toate abonamentele pe acel ID. Pentru abonamentele cu store-and-forward
setat pe ON, serverul trimite toate mesajele netrimise de la ultima conectare a clientului. In acelasi timp, toate mesajele
inutile sunt sterse din memorie, actualizand fiecare abonament dupa acest proces. Ultimul caz distins la conectare este
cel in care IDul este nou, caz in care serverul adauga la clients noul client. La primirea unui pachet UDP, se incadreaza
in structura de UDPpacket si se trimite un ack inapoi, urmand ca pachetul sa fie procesat. In cadrul procesarii rezulta
o structura de tip message, care contine IPul si portul sursei, urmat de pachetul primit. Initial serverul cauta ca topicul
mesajului sa fie existent in memorie. Daca nu este, acesta doar creaza topicul si arunca mesajul deoarece nimeni nu poate fi 
abonat pe un topic inexistent in memorie. Daca este gasit topicul, mesajul este trimis tuturor clientilor abonati conectati
si este memorizat doar daca a fost gasit ulterior un client abonat cu store-and-forward care este deconectat, caz in care
la reconectarea acelui client serverul trimite mesajul si il sterge eventual din memorie. Cand serverul primeste un pachet
TCP pe unul dintre socketii de conexiune activa cu clientii, acesta initial localizeaza clientul in lista sa interna de
clienti in functie de descriptorul pe care primeste mesajul. Apoi, verifica daca mesajul primit marcheaza deconectarea
clientului (printr-un pachet de lungime 0), caz in care se inchide socketul de conexiune si se elimina din follfds intrarea
respectiva, anuntand ulterior la stdout acest lucru. Daca nu este cazul unei deconectari, mesajul primit se incadreaza
intr-o structura de tipul TCPreq a carui camp de tip ramifica firul de executie al serverului in 3 directii. Pentru tipul
0 serverul trateaza comanda ca un unsubscribe, eliminand din lista de abonamente pentru topicul primit orice intrare
cu IDul clientului de la care a primit pachetul. La tipul 1 serverul preia pachetul ca o cerere de abonare fara 
store-and-forward, iar la tipul 2 abonarea se face cu SF enabled. In ambele cazuri se cauta in memorie daca exista deja
un abonament pe IDul clientului, iar daca da se actualizeaza modul abonarii. Pentru o prima abonare la un topic existent
facuta de un client, doar se adauga la lista de abonamente. In mod aditional, daca topicul nu este gasit in memorie, 
acesta este creat simultan cu noul abonament introdus in capul listei acestui topic.

Un subscriber este rulat cu ./subscriber <id> <ip> <port>. Validarea argumentelor se face similar cu serverul, iar partea
de setup este mai simplista, facandu-se doar un connect pe IP-ul si portul primite care parametrii ai programului, folosind
un socket TCP reutilizabil fara algoritmul lui Nagle. De data aceasta vectorul pollfds are dimensiune fixa egala cu 2,
clientul lucrand doar cu socketul de conexiune cu serverul si intrarea standard. Acestea sunt parcurse similar cu serverul
intr-un loop principal infinit. Pentru un pachet primit de la server, clientul verifica daca este un pachet de sesizare a
inchiderii conexiunii, caz in care si acesta de opreste. Altfel, pachetul este incadrat initial intr-un intreg ce serveste
rolul intelegerii client-server pentru lungimea mesajului trimis. Astfel, comunicarea dintre acestia este una eficienta si
nu incarca in mod inutil reteaua cu date. Dupa procesarea lungimii mesajul este trimis de server in marime strict minimala
din punct de vedere al payloadului (contentul din pachetul initial UDP) si este incadrat intr-o structura de tip mesaj pe
care clientul o afiseaza la stdout prin functia printMessage(). Cand clientul primeste ceva de pe intrarea standard, el
incepe construirea unui mesaj de tip TCPreq care este trimis serverului doar daca comanda este valida, iar in cazul
comenzii exit, programul se incheie dupa ce socketul este inchis, fapt ce este propagat ulterior catre server.
