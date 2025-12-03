
### Strategia di Scheduling Gerarchico ("Rocks & Sand")

[cite_start]**Obiettivo:** Risolvere il problema dei **"False Ties" (Falsi Pareggi)** citato esplicitamente nelle slide come limitazione dell'algoritmo[cite: 658, 660]. L'algoritmo originale, in caso di priorità $F(u)$ simile, sceglie arbitrariamente (es. alfabeticamente), portando a risultati subottimali.

Abbiamo introdotto un sistema di decisione a **3 Livelli** per rompere i pareggi in modo intelligente.

-----

#### Livello 1: La Forza Principale (Urgenza Temporale & Risorse)

[cite_start]Questo è il livello base richiesto dai professori e dalle slide[cite: 81].

**Formula:**
$$F(u) = S_{norm}(u) \cdot (C_{avg}(u) + \epsilon)$$

  * [cite_start]**$S_{norm}$ (Slack):** Misura l'urgenza temporale basata sulla mobilità[cite: 87].
  * **$C_{avg}$ (Congestione):** Misura l'impatto sulle risorse (modificato da Max a Avg come richiesto dai prof).
  * **Regola:** Si schedula il nodo con **$F(u)$ Minore**.

-----

#### Livello 2: Tie-Breaker Primario (Granularità Quadratica)

**Intuizione:** *"I Sassi prima della Sabbia"*.
Se due nodi hanno la stessa priorità $F(u)$, dobbiamo capire quale dei due sblocca il percorso geometricamente più difficile da incastrare ("Tetris").
Un percorso con un'operazione da 3 cicli (monolitica) è più rigido di uno con tre operazioni da 1 ciclo (frammentate), anche se la durata totale è uguale. $S$ e $C$ non vedono questa differenza.

**Formula ($\tau$):**
Calcoliamo la somma dei quadrati delle latenze sul **percorso critico a valle** che parte da un successore di $u$.
Sia $\Pi(u)$ l'insieme di tutti i percorsi possibili che partono dai figli di $u$ fino al pozzo (sink).

$$\tau(u) = \max_{p \in \Pi(u)} \left( \sum_{v \in p} \text{latency}(v)^2 \right)$$

  * **Perché il Quadrato ($L^2$)?** Penalizza in modo non lineare le operazioni lunghe.

      * Percorso A (2 ops da 2 cicli): $2^2 + 2^2 = 8$.
      * Percorso B (1 op da 3 cicli + 1 da 1 ciclo): $3^2 + 1^2 = 10$.
      * **Risultato:** $10 > 8$. Vince B. Scheduliamo B per primo perché contiene il blocco "più grosso".

  * **Regola:** A parità di $F(u)$, si sceglie il nodo con **$\tau(u)$ Maggiore**.

-----

#### Livello 3: Tie-Breaker Secondario (Potenziale di Sblocco)

**Intuizione:** *"Massimizzare il Flusso"*.
Se due nodi hanno stessa priorità $F$ e stessa difficoltà geometrica $\tau$, allora guardiamo la **quantità**. È preferibile il nodo che sblocca il maggior numero totale di operazioni a valle, per aumentare il parallelismo disponibile (la "sabbia" per riempire i buchi futuri).

**Formula ($N_{desc}$):**
$$N_{desc}(u) = | \{v \in V \mid u \text{ è antenato di } v \} |$$

  * **Regola:** A parità di $F(u)$ e $\tau(u)$, si sceglie il nodo con **$N_{desc}(u)$ Maggiore**.

-----

### Riepilogo dell'Algoritmo di Decisione

Nel ciclo del *List Scheduler*, la `ReadyList` viene ordinata utilizzando questa tupla di chiavi:

1.  **Minimizzare $F(u)$** (Urgenza & Congestione - *Standard Force*)
2.  **Massimizzare $\tau(u)$** (Difficoltà Geometrica - *Tua Idea*)
3.  **Massimizzare $N_{desc}(u)$** (Parallelismo - *Tua Idea*)

