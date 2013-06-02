#include "game.h"

void Game::onConnect(unsigned id) {
  LOG("Cube %d connected\n", id);

  vid[id].initMode(BG0_SPR_BG1);
  vid[id].attach(id);

  vid[id].bg0.image(vec(0,0), Background);
}

void Game::onNeighborAdd(unsigned firstID, unsigned firstSide,
                         unsigned secondID, unsigned secondSide) {

}

void Game::onNeighborRemove(unsigned firstID, unsigned firstSide,
                   unsigned secondID, unsigned secondSide) {

}

void Game::title() {
  Events::cubeConnect.set(&Game::onConnect, this);
  Events::neighborAdd.set(&Game::onNeighborAdd, this);
  Events::neighborRemove.set(&Game::onNeighborRemove, this);

  for (CubeID cube : CubeSet::connected()) {
    onConnect(cube);
  }
};

void Game::init() {

}

void Game::run() {
  while(1) {
    System::paint();
  }
}

void Game::cleanup() {

}