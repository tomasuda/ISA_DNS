## Rozšírenie

- Podpora dotazov typu A (IPv4)
- Podpora dotazov typu AAAA (IPv6)
- Spracovanie dotazov na root doménu (.)
- Validácia doménových mien (odmietnutie neplatných názvov)

## Testovanie

Pri testoch je nutné mať podporu pre IPv6
Musia byť nainštalované: python3, pytest, dnsutils

Testy sú spustiteľné príkazom `make test`