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

## Odevzdání

- Odevzdávají se zdrojové kódy (`*.c`, `*.h`) a soubor `Makefile` zabaleny do archivu `proj2.zip`.
- Archiv musí obsahovat soubor `Makefile` na stejné úrovni jako samotný archiv.
- Odevzdání probíhá prostřednictvím informačního systému.
