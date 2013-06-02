#include "game.h"

static AssetSlot MainSlot = AssetSlot::allocate()
    .bootstrap(GameAssets);

void Game::onConnect(unsigned cid) {
  if (ready) {
    return;
  }
  // this cube is either new or reconnected
  if (lostCubes.test(cid)) {
    // this is a reconnected cube since it was already lost this paint()
    lostCubes.clear(cid);
    reconnectedCubes.mark(cid);
  } else {
    // this is a brand-spanking new cube
    newCubes.mark(cid);
  }
  // begin showing some loading art (have to use BG0ROM since we don't have assets)
  dirtyCubes.mark(cid);
  auto& g = vid[cid];
  g.attach(cid);
  g.initMode(BG0_ROM);
  g.bg0rom.fill(vec(0,0), vec(16,16), BG0ROMDrawable::SOLID_BG);
  g.bg0rom.text(vec(1,1), "Hold on!", BG0ROMDrawable::BLUE);
  g.bg0rom.text(vec(1,14), "Adding Cube...", BG0ROMDrawable::BLUE);
}

void Game::onRefresh(unsigned cid) {
  if (ready) {
    return;
  }
  LOG("refresh: %d\n", cid);
  dirtyCubes.mark(cid);
}

void Game::activateCube(CubeID cid) {
  // mark cube as active and render its canvas
  activeCubes.mark(cid);
  vid[cid].initMode(BG0_SPR_BG1);

  if (cid == 0) {
    vid[cid].bg0.image(vec(0,0), WaitingScreen);
  } else {
    vid[cid].bg0.image(vec(0,0), RoomBackground);

    auto &bg1 = vid[cid].bg1;

    bg1.setMask(BG1Mask::filled(vec(4, 4), vec(8, 8)));
    bg1.image(vec(4,4), CheckMark);
    bg1.setPanning(vec(96,0));
  }

  // TODO: we may or may not have to do this
  // auto neighbors = vid[cid].physicalNeighbors();
  // if (neighbors.hasNeighborAt(Side(0))) {
  //   showConnected(cid, Side(side));
  // }
}

void Game::checkConnection(unsigned firstID, unsigned firstSide,
                           unsigned secondID, unsigned secondSide) {
  if (firstID == 0 && firstSide != 2) {
    showConnected(secondID);
  } else if (secondID == 0 && secondSide != 2) {
    showConnected(firstID);
  }
}

void Game::showConnected(CubeID cid) {
  LOG("Connected and ready\n");
  readyCubes++;
  vid[cid].bg1.setPanning(vec(4,4));
}

void Game::hideConnected(CubeID cid) {
  readyCubes--;
  vid[cid].bg1.setPanning(vec(96,0));
}

void Game::onNeighborAdd(unsigned firstID, unsigned firstSide,
                     unsigned secondID, unsigned secondSide) {
  if (ready) {
    return;
  }
  if (isActive(firstID) && isActive(secondID)) {
    checkConnection(firstID, firstSide, secondID, secondSide);
  }
}

void Game::onNeighborRemove(unsigned firstID, unsigned firstSide,
                   unsigned secondID, unsigned secondSide) {
  if (ready) {
    return;
  }
  if (firstID == 0 || secondID == 0) {
    unsigned id = firstID == 0 ? secondID : firstID;
    hideConnected(id);
  }
}

bool Game::isActive(NeighborID nid) {
  return nid.isCube() && activeCubes.test(nid);
}

void Game::onTap(unsigned id)
{
	CubeID cube(id);

	if (cube.isTouching()) {
		switch(crew[id]) {
		case SHIP:
			LOG("Ship is being tapped. What does this do again?\n");
			break;
		case CAPTAIN:
			if (obstacleEncountered && currentObstacle == ASTEROID) {
				if (energies[id] >= 200){
					LOG("Firing lasers! Success! Asteroid destoyed!\n");
					energies[id] -= 200;
					FinishObstacle();
				}
				else {
					LOG("Not enough power to fire the lasers!");
				}
			}
			else if (obstacleEncountered && currentObstacle == ALIEN) {
				if (energies[id] >= 500){
					LOG("Firing lasers! Success! Alien spacecraft destoyed!\n");
					energies[id] -= 500;
					FinishObstacle();
				}
				else {
					LOG("Not enough power to fire the lasers!");
				}
			}
			else {
				LOG("There's nothing to fire at!\n");
			}
			LOG("Current state: %d\n", energies[id]);
			break;
		case ENGINEER:
			LOG("Engineer generating power!\n");
			energies[id] += 5;
			LOG("Current state: %d\n", energies[id]);
			break;
		default:
			break;
		}
	}

	if (crew[id] == SCIENTIST) {
		if (cube.isTouching()) {
			shieldDrain = true;
		}
		else {
			shieldDrain = false;
		}
		LOG("Current state: %d\n", energies[id]);
	}
}

void Game::waitForPlayers() {
  config.append(MainSlot, GameAssets);
    loader.init();

  Events::cubeConnect.set(&Game::onConnect, this);
  Events::cubeRefresh.set(&Game::onRefresh, this);

  Events::neighborAdd.set(&Game::onNeighborAdd, this);
  Events::neighborRemove.set(&Game::onNeighborRemove, this);

  for(CubeID cid : CubeSet::connected()) {
    vid[cid].attach(cid);
    activateCube(cid);
  }

  while (readyCubes < 3) {
    draw();
  }

  for (unsigned i = 0; i < kNumCubes; i++) {
    vid[i].bg1.eraseMask();
  }

  ready = true;

  // TODO: unbind waiting stuff

  Events::cubeConnect.unset();
  Events::cubeRefresh.unset();

  Events::neighborAdd.unset();
  Events::neighborRemove.unset();
}

void Game::draw() {
  // clear the palette
  newCubes.clear();
  lostCubes.clear();
  reconnectedCubes.clear();
  dirtyCubes.clear();

  // fire events
  System::paint();

  // dynamically load assets just-in-time
  if (!(newCubes | reconnectedCubes).empty()) {
      // AudioTracker::pause();
      // playSfx(SfxConnect);
      loader.start(config);
      while(!loader.isComplete()) {
          for(CubeID cid : (newCubes | reconnectedCubes)) {
              vid[cid].bg0rom.hBargraph(
                  vec(0, 4), loader.cubeProgress(cid, 128), BG0ROMDrawable::ORANGE, 8
              );
          }
          // fire events while we wait
          System::paint();
      }
      loader.finish();
      // AudioTracker::resume();
  }

  // repaint cubes
  for(CubeID cid : dirtyCubes) {
      activateCube(cid);
  }

  // also, handle lost cubes, if you so desire :)
}

void Game::init() {
  // set background
  // set ship
  // setup obstacles?
  // setup projectiles?


  Events::cubeTouch.set(&Game::onTap, this);
  Events::neighborAdd.set(&Game::onNeighborAdd, this);
  Events::neighborRemove.set(&Game::onNeighborRemove, this);
}

void Game::run() {
	rndm = Random();

  vid[0].bg0.image(vec(0, 0), Stars);

	for (int i = 0; i < kNumCubes; i++) {
		energies[i] = 1280;
		switch (i) {
		case 0:
			crew[i] = SHIP;
      vid[i].bg1.setMask(BG1Mask::filled(vec(0, 4), vec(8, 8)));
      vid[i].bg1.image(vec(0,4), Ship, 0);
			obstacles[i] = ALIEN;
			break;
		case 1:
			crew[i] = CAPTAIN;
      vid[i].bg1.setMask(BG1Mask::filled(vec(7, 8), vec(4, 8)));
      vid[i].bg1.image(vec(7, 8), Captain, 0);
			obstacles[i] = ASTEROID;
			break;
		case 2:
			crew[i] = ENGINEER;
      vid[i].bg1.setMask(BG1Mask::filled(vec(7, 8), vec(4, 8)));
      vid[i].bg1.image(vec(7, 8), Engineer, 0);
			obstacles[i] = IONSTORM;
			break;
		case 3:
			crew[i] = SCIENTIST;
      vid[i].bg1.setMask(BG1Mask::filled(vec(7, 8), vec(4, 8)));
      vid[i].bg1.image(vec(7, 8), Scientist, 0);
			break;
		}
	}

	obstacleEncountered = false;
	disasterAvoided = false;
	shieldCharge = 0;

	TimeStep ts;

	// run gameplay code
	while(1) {
		Update(ts.delta());
		System::paint();
		ts.next();
	}
}

void Game::Update(TimeDelta timeStep){
	if (shieldDrain) {
		energies[3] -= 5;
		shieldCharge += 5;
		LOG("Shield Power: %d\n", energies[3]);
		if (currentObstacle == IONSTORM && shieldCharge >= 100) {
			LOG("ACTIVATING SHIELDS! You have passed through the ion cloud safely!");
			FinishObstacle();
		}

		if (currentObstacle == ALIEN && shieldCharge >= 200) {
			LOG("ACTIVATING SHIELDS! You were safely shielded from the alien's weapons!");
			FinishObstacle();
		}
	}

	if (!obstacleEncountered) {
		obstacleTimer -= timeStep.seconds();
	}

	else {
		reactionTimer -= timeStep.seconds();
	}

	if (obstacleTimer <= 0.0f) {
		obstacleTimer = timeBetweenObstacles;
		currentObstacle = obstacles[rndm.randint(0,2)];
		switch (currentObstacle) {
		case ALIEN:
			LOG("ALIEN ENCOUNTERED!\n");
			break;
		case ASTEROID:
			LOG("ASTEROID ENCOUNTERED!\n");
			break;
		case IONSTORM:
			LOG("ION STORM ENCOUNTERED!\n");
			break;
		default:
			break;
		}
		obstacleEncountered = true;
	}

	else if (reactionTimer <= 0.0f) {
		FinishObstacle();
		LOG("RESOLVED!\n");
	}
}

void Game::FinishObstacle() {
	shieldCharge = 0;
	obstacleEncountered = false;
	obstacleTimer = timeBetweenObstacles;
	reactionTimer = timeToReactToObstacle;
	currentObstacle = NONE;
}

void Game::cleanup() {

}
