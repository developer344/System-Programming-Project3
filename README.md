# System Programming - Project 3 - 2021

## Name: Ioannis
## Surname: Georgopoulos
## sdi:1115201800026

Το readme.txt περιέχει σχόλια όσων αναφορά την αποστολή δεδομένων μέσω pipes καθώς και σχολιασμό πανώ στην λειτουργία των signals τόσο στο travelmonitorClient όσο και στα monitorsServers

Sockets:
========
Για την σύνδεση χρησιμοποίησα ports από το 9000 και πάνω


Αποστολή integers:
------------------
Αν το socketufferSize είναι μεγαλύτερο ή ίσο του 4 απλά στένλω το int με sizeof(int)=4bytes. Αν είναι 
μικρότερο του 4 και δεδομένου ότι το μικρότερο μέγεθος buffer που μπορεί να βάλεi ο χρήστης είναι 2 όπως
αναφέρθηκε στο piazza, χωρίζω το int σε δυο κομμάτια και τα αποθηκεύω σε short ints(2bytes) και μετά τα 
στέλνω με sizeof(short)=2bytes και από την πλευρά τα επανασυνθέτω με bitwise or.

Αποστολή strings:
------------------
Για την αποστολή strings μέσω sockets αν το string είναι μεγαλυτερο του bufferSize χωρίζω το string σε 
μικρότερα κομμάτια μεγέθους buffersize και αν υπαρχει υπόλοιπο το τελευταίο κομμάτι στέλνεται με size όσο
το κομμάτι που απομένει. Αν το string είναι μικρότερο του socketBfferSize απλά στέλνει το string όπως έχει
με size=strlen(string). Ο παραλήπτης λαμβάνει πρώτα μέσω socket το μέγεθος του string βλέπει σε πόσα κομμάτια
χωρίστηκε και έπειτα δίαβαζει ένα ένα τα κομμάτια και στο τέλος αν υπάρχει υπόλοιπο διαβάζει και το υπόλοιπο.
Στέλνω το μέγεθος του string για να ξέρει ο παραλήπτης πόσα reads να κάνει.

Αποστολή bloomfilter:
---------------------
Για την αποστολή βλέπω πόσα byte είναι το bloomfilter και το στέλνω σαν string. Η διαφορά με τα strings
είναι ότι στέλνω ολα τα bytes του bloomfilter oxi mono ekei pou teleiwnei to string. Μια διευκρίνηση είναι
οτι το travelmonitorClient εχει ένα bloomfilter ανα monitorsServer.Έτσι είχε πει η βοηθός οτι μπορεί να
γινει στην δευτερη εργασία και έτσι δεν το άλλαξα και σε αυτη την εργασία αφού οι λειτουργίες που ικανοποιεί
είναι ίδιες με αυτές της δεύτερης εργασίας.

Threads:
========
Xρησιμοποίησα το struct thread_input ώστε να μπορεί το main thread να περάσει τα arguments που χρειάζονται
τα υπόλοιπα threads.

Στα threads χρησιμοποιησα mutexes στα critical sections εκεί που δυο threads ανταγωνίζονταν για τον ίδιο πόρο
ταυτόχρονα.

Τα threads ελέγχουν αν έχει αδειάσει ο cyclic buffer και όταν αδειάσει τερματιίζουν.


Μεταγλωτηση και εκτέλεση:
=========================
Η μεταγλώτηση γίνεται με το makeFile
Η εκτέλεση γίνεται με το ./bin/travelMonitorClient -m numMonitors -b socketBufferSize -c cyclicBufferSize -s sizeOfBloom -i input_dir -t numThreads
