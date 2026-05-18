/*
 * Game.cpp
 *
 *  Created on: Apr 17, 2018
 *      Author: VIDEOJUEGOS UTALCA
 */
#include <chrono>
#include <thread>
#include "Game.h"
#include <algorithm>
#include "KeyboardController.h"
#include "RandomController.h"
#include "SimpleController.h"
#include "SimplePacmanController.h"
#include "BTGhostController.h"
#include "FSMController.h"
#include "Ghost.h"
#include "BlinkyController.h"
#include "PinkyController.h"
#include "SueController.h"
#include "PacmanController.h"
#include "DecisionTreeGhostController.h"
#include "PinkyBTController.h"
#include "PacmanBTController.h"
#include "PinkyFSMController.h"
#include "ClydeFSMController.h"
#include "InkyBTController.h"


extern bool quick;
extern bool nogui;

Game::Game():currentMap(0),
filenames{"mazes/a.txt","mazes/b.txt","mazes/c.txt","mazes/d.txt"},
gameState(filenames[currentMap]),
gv(std::make_unique<GameView>(std::vector<std::string>{"images/maze-a.png","images/maze-b.png","images/maze-c.png","images/maze-d.png"})) {

	auto pacman=std::make_shared<MsPacMan>(gameState.getMaze().getPacmanStart());
	gameState.addPacMan(pacman);
	//pacmanControl=std::make_shared<KeyboardController>(pacman);
	//pacmanControl=std::make_shared<SimplePacmanController>(pacman);
	// pacmanControl=std::make_shared<PacmanController>(pacman);
	pacmanControl = std::make_shared<PacmanBTController>(pacman);
	std::vector<std::shared_ptr<Ghost>> ghosts;
	for(int i=0;i<4;i++){
		auto ghost=std::make_shared<Ghost>(gameState.getMaze().getGhostStart()[i]);
		ghosts.push_back(ghost);
	}
	gameState.addGhosts(ghosts);
	ghostsControl.push_back(std::make_shared<PinkyFSMController>(ghosts[0]));
	ghostsControl.push_back(std::make_shared<InkyBTController>(ghosts[1]));
	ghostsControl.push_back(std::make_shared<PinkyBTController>(ghosts[2]));
	ghostsControl.push_back(std::make_shared<ClydeFSMController>(ghosts[3]));
	//  ghostsControl.push_back(std::make_shared<BlinkyController>(ghosts[0]));
	//  ghostsControl.push_back(std::make_shared<InkyController>(ghosts[1]));
	//  ghostsControl.push_back(std::make_shared<PinkyController>(ghosts[2]));
	//  ghostsControl.push_back(std::make_shared<SueController>(ghosts[3]));
}

const int NOSCORELIMIT = 10000;
void Game::run(){
	int lastScore=gameState.getScore();
	int framesWithoutChange=0;
	while(true){
		if(!nogui)
			gv->draw(currentMap,gameState);
		gameState.updatePacman(pacmanControl->getMove(gameState));
		gameState.updateEaten();
		std::vector<Move> ghostMoves;
		std::transform(ghostsControl.begin(), ghostsControl.end(), std::back_inserter(ghostMoves), [this](const std::shared_ptr<Controller> &ghost) { return ghost->getMove(gameState);});
		gameState.updateGhosts(ghostMoves);
		gameState.updateEaten();
		if(gameState.won()){
			currentMap=(currentMap+1)%filenames.size();
			gameState.reset(filenames[currentMap]);
		}else if(gameState.lost()){
			if(!nogui){
				std::cout<<"Aborting because of no progress."<<std::endl;
				std::cout<<"Final score: "<<lastScore<<std::endl;
			}else{
				std::cout<<lastScore<<std::endl;
			}
			break;
		}
		if(lastScore==gameState.getScore()){
			framesWithoutChange++;
		}else{
			lastScore=gameState.getScore();
			framesWithoutChange=0;
		}
		if(framesWithoutChange==NOSCORELIMIT){
			
			if(!nogui){
				std::cout<<"Aborting because of no progress."<<std::endl;
				std::cout<<"Final score: "<<lastScore<<std::endl;
			}else{
				std::cout<<lastScore<<std::endl;
			}
			break;
		}
		if(!quick)
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}


}


