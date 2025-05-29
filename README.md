# MultiPandOS

To compile the project:
```bash
mkdir -p build && pushd build && cmake .. && make && popd
```

Implementazione Scheduler:
Lo Scheduler gestisce la coda ready tramite MUTEX_GLOBAL, in modo da accedere in mutua esclusione alla coda dei processi pronti evitando race condition tra le CPU; viene usato un ciclo while infinito perché, una volta avviato, deve restare pronto a disptachare nuovi processi o entrare in idle finché non ne arrivano di nuovi, delegando al timer e agli interrupt l’innesco di ogni nuovo dispatch.
