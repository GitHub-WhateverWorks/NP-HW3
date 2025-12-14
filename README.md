| Name | 何柏翰 |
| Id | 112550028 |

# Network Programming HW 3

- This assigment contains 3 distinct code bases: player, developer, and server.
- player: player_client, developer: developer_client, server: server

## Server

- The Makefile outside of the 3 code bases is for Server

- Environment setup: sudo apt install unzip (However, this should already be installed, just a foolproof)

- Change directory to {root}/, and do " make clean && make "

- After make is complete, 2 binary files lobby_server and dev_server should be in root/bin/

- Do bin/dev_server in root/ directory to start developer server on port 15000

- Do bin/lobby_server in root/ directory to start lobby server on port 17000

- Both server should be ran on CSIT, and will persists without any interaction.

## Developer

- Environment setup: sudo apt install libsfml-dev, sudo apt install unzip (libsfml-dev is for SFML, all gui in this assignment is made using SFML)

- Change directory to developer_client/, and do "make clean && make "

- After make is complete, 1 binary file developer_client_gui should be in developer_client/ directory

- Do ./developer_client_gui <ip> <port> in developer_client/ directory to start developer client. For instance, "./developer_client_gui 140.113.17.14 15000"

- Within the gui, register if you don't have an account, or login if you do.

- To upload a new game, the directoy should be relative, for example, in developer_client/games/ there is a file bombarena.zip ready to be uploaded, the input should be ./games/bombarena.zip

- Click the My Games button to either update your own games, or soft delete them. Soft delete causes players to be unable to start new rooms.

- A window will pop up to ensure you know the risk of deleting a game.

## Player

- Environment setup: sudo apt install libsfml-dev, sudo apt install unzip (libsfml-dev is for SFML, all gui in this assignment is made using SFML)

- Change directory to player_client/, and do "make clean && make "

- After make is complete, 1 binary file game_store_gui should be in player_client/ directory

- Do ./game_store_gui <ip> <port> in player_client/ directory to start player client. For instance, "./game_store_gui 140.113.17.14 17000"

- Within the gui, register if you don't have an account, or login if you do.

- For every game you can see the comments below, with a review button to add reviews.

- Every game will show your current downloaded version and the newest version, only the newest version can play the game.

- If you have hosting, click create room. If you are joining, ask the host for the ROOM CODE on the TOP LEFT CORNER of the room, and insert the number into the ROOM CODE section to the right of JOIN ROOM button.

- Once every player is in room, the host can start the game by pressing Start.