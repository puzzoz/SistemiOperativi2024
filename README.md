# MultiPandOS

To compile the project:
```bash
mkdir -p build && pushd build && cmake .. && make && popd
```

Divisione funzioni:

**Gruppo 1: Inizializzazione e gestione base dei PCB - Elena**
- **initPcbs()** – Inizializza i PCB. Analogamente all'inizializzazione dell'ASL, questa funzione si occupa di preparare la struttura per i PCB.
- **freePcb(pcb_t* p)** – Dealloca un PCB. Si occupa di liberare la memoria associata a un processo, un'operazione di gestione delle risorse.
- **insertProcQ(struct semAdd* head, pcb_t* p)** – Inserisce un PCB in una coda di processi, una funzione di manipolazione della lista che non è troppo complessa.
- **mkEmptyProcQ(struct semAdd* head)** - Creare una coda vuota di processi è una semplice inizializzazione di una lista.

**Gruppo 2: Gestione dei semafori - Eric**
- **initASL()** – Inizializza la struttura dati dell'ASL (Active Semaphore List). Questa funzione è probabilmente legata alla configurazione iniziale e quindi meno complessa.
- **insertBlocked(int* semAdd, pcb_t* p)** – Inserisce un PCB in una lista di processi bloccati associata a un semaforo.
- **removeBlocked(int* semAdd)** – Rimuove un PCB dalla lista dei bloccati associata a un semaforo.
- **headBlocked(int* semAdd)** – Restituisce il primo PCB dalla lista di bloccati di un semaforo, un'operazione di lettura della testa della lista.
- **outBlockedPid(int pid)** – Rimuove un processo dalla lista dei bloccati usando l'ID del processo, una funzione di rimozione basata su un criterio specifico.
- **outBlocked(pcb_t* p)** – Rimuove un PCB dalla lista dei bloccati, una funzione di rimozione generica da una lista.

**Gruppo 3: Gestione dei processi figli - Mattia**
- **emptyChild(pcb_t* p)** – Verifica se un processo ha figli. Un'operazione di controllo della lista dei figli.
- **insertChild(pcb_t* prnt, pcb_t* p)** – Inserisce un PCB come figlio di un altro processo. Si tratta di una manipolazione della lista dei figli.
- **removeChild(pcb_t* p)** – Rimuove un figlio da un processo. Gestisce la rimozione da una lista dei figli.
- **outChild(pcb_t* p)** – Rimuove un processo figlio dalla lista dei figli di un altro processo. È una funzione di rimozione come la precedente, ma più specifica.
- **allocPcb()** - Allocare un PCB è una semplice operazione di allocazione di memoria, probabilmente usando malloc o una struttura simile.

**Gruppo 4: Gestione delle code di processi - Andrea**
- **removeProcQ(struct semAdd* head)** – Rimuove un PCB dalla coda dei processi. Una funzione di rimozione dalla coda.
- **headProcQ(struct semAdd* head)** – Restituisce il primo PCB dalla coda dei processi. Operazione simile alla lettura della testa di una lista.
- **outProcQ(struct semAdd* head, pcb_t* p)** – Rimuove un PCB specifico dalla coda dei processi. Richiede la rimozione di un nodo specifico.
- **emptyProcQ(struct semAdd* head)** - Verificare se una coda è vuota è un'operazione diretta di controllo delle condizioni sulla lista.
