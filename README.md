# IOS-projekt-2-synchronizace-
Pošta
# Projekt 2 - Synchronizace (IOS)

Tento projekt je inspirován knihou "The Little Book of Semaphores" od Allena B. Downeyho a řeší problém poštovního úřadu.

## Popis úlohy

V systému jsou tři typy procesů: hlavní proces, poštovní úředníci a zákazníci. Zákazníci přicházejí na poštu s požadavky na služby (listovní služby, balíky, peněžní služby). Každý úředník obsluhuje všechny fronty a vybírá náhodně jednu z front. Po uzavření pošty úředníci dokončí obsluhu všech zákazníků ve frontě a po vyprázdnění všech front odchází domů.

## Spuštění

$ ./proj2 NZ NU TZ TU F


- NZ: počet zákazníků
- NU: počet úředníků
- TZ: maximální čas v milisekundách, po který zákazník čeká, než vejde na poštu (0 <= TZ <= 10000)
- TU: maximální délka přestávky úředníka v milisekundách (0 <= TU <= 100)
- F: maximální čas v milisekundách, po kterém je uzavřena pošta pro nově příchozí (0 <= F <= 10000)

## Implementační detaily

- Používá se sdílená paměť pro čítač akcí a sdílené proměnné pro synchronizaci.
- Synchronizace mezi procesy je řešena pomocí semaforů.
- Každý proces zapisuje informace o svých akcích do souboru `proj2.out`.
- Nepoužívá se aktivní čekání (včetně cyklického časového uspání procesu) pro účely synchronizace.
- Projekt je implementován v jazyce C.

## Překlad

- Překlad se provede pomocí nástroje `make` a souboru `Makefile`.
- Po překladu vznikne spustitelný soubor `proj2`.
- Překladové přepínače: `-std=gnu99 -Wall -Wextra -Werror -pedantic`


- NZ: počet zákazníků
- NU: počet úředníků
- TZ: maximální čas v milisekundách, po který zákazník čeká, než vejde na poštu (0 <= TZ <= 10000)
- TU: maximální délka přestávky úředníka v milisekundách (0 <= TU <= 100)
- F: maximální čas v milisekundách, po kterém je uzavřena pošta pro nově příchozí (0 <= F <= 10000)


## Příklad výstupu

Příklad výstupního souboru `proj2.out` pro následující příkaz:

$ ./proj2 3 2 100 100 100

1: U 1: started
2: Z 3: started
3: Z 1: started
4: Z 1: entering office for a service 2
5: U 2: started
6: Z 2: started
7: Z 3: entering office for a service 1
8: Z 1: called by office worker
9: U 1: serving a service of type 2
10: U 1: service finished
11: Z 1: going home
12: Z 3: called by office worker
13: U 2: serving a service of type 1
14: U 1: taking break
15: closing
16: U 1: break finished
17: U 1: going home
18: Z 2: going home
19: U 2: service finished
20: U 2: going home
21: Z 3: going home

