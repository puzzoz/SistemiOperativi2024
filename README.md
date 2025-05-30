# MultiPandOS

To compile the project:
```bash
mkdir -p build && pushd build && cmake .. && make && popd
```

Implementazione Scheduler:
Lo Scheduler gestisce la coda ready tramite MUTEX_GLOBAL, in modo da accedere in mutua esclusione alla coda dei processi pronti evitando race condition tra le CPU; viene usato un ciclo while infinito perché, una volta avviato, deve restare pronto a disptachare nuovi processi o entrare in idle finché non ne arrivano di nuovi, delegando al timer e agli interrupt l’innesco di ogni nuovo dispatch.

Implementazione Interrupt:
La gestione degli interrupt è suddivisa in tre funzioni distinte: handlePLTInterrupt, handlePseudoClockInterrupt e handleDeviceInterrupt, coordinate centralmente da dispatchInterrupt. Tale separazione migliora la chiarezza del sistema e ne facilita l’estendibilità. Tutti i blocchi critici condivisi sono protetti da MUTEX_GLOBAL, prevenendo race condition in ambiente multiprocessore; ogni accesso a variabili globali avviene quindi in mutua esclusione.
In handleDeviceInterrupt, la linea dell’interrupt viene determinata a partire dal codice dell’eccezione; tramite questa e il numero di device, viene ricavato l’indice del semaforo con getDeviceIndex(). Un controllo successivo sul valore di device_number, dopo l’analisi del bitmap, evita accessi a indici non validi nel caso di bitmap inconsistente. Lo stato del device viene restituito al processo sbloccato prima della ripresa dell’esecuzione.
La funzione handlePLTInterrupt utilizza un modulo esterno per aggiornare il tempo di utilizzo CPU del processo interrotto.

Implementazione Exception Handler:
La funzione exceptionHandler, di cui passare l'indirizzo nei descrittori dei processi, svolge la funzione di entrypoint per lo smistamento dell'eccezione leggendone lo stato dall'indirizzo di memoria dedicato e occupandosi poi di reindirizzare l'esecuzione a funzioni più specializzate a seconda dei valori letti che definiscono la richiesta del processo chiamante o l'errore che ha generato la chiamata