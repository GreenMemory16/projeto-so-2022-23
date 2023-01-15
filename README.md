# projeto-so-2022-23

Código fornecido para o segundo exercício do projeto de Sistemas Operativos do ano letivo 2022-2023.

Consultar o [enunciado do projeto](https://github.com/tecnico-so/enunciado-proj-so-2022-23).

## Casos especiais

- Quando o processo do subscriber termina, a sua worker thread não é terminada até que um publisher volte a mandar uma mensagem.
https://piazza.com/class/l92u0ocmbv05rk/post/108

- O publisher só termina quando tenta escrever num pipe que já foi fechado pelo mbroker. Isto signfica que ficará à espera de input do utilizador, mesmo se o mbroker já tiver terminado a sua worker thread.
https://piazza.com/class/l92u0ocmbv05rk/post/88