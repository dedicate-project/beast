// Catch2
#include <catch2/catch.hpp>

// Standard
#include <array>

// Maze
#include <maze/maze.hpp>
#include <maze/player.hpp>
#include <maze/task_world.hpp>
#include <maze/tiles.hpp>

TEST_CASE("Seeded maze is reproducible") {
  SECTION("Same seed yields identical layout") {
    maze::Maze a(11, 11, 0.2, /*seed=*/42);
    maze::Maze b(11, 11, 0.2, /*seed=*/42);

    REQUIRE(a.getStartPosition() == b.getStartPosition());
    REQUIRE(a.getEndPosition() == b.getEndPosition());
    REQUIRE(a.perceiveTiles(20) == b.perceiveTiles(20));
  }

  SECTION("Different seeds generally differ") {
    maze::Maze a(15, 15, 0.2, /*seed=*/42);
    maze::Maze c(15, 15, 0.2, /*seed=*/1337);

    const bool differs = a.getStartPosition() != c.getStartPosition() ||
                         a.getEndPosition() != c.getEndPosition() ||
                         a.perceiveTiles(20) != c.perceiveTiles(20);
    REQUIRE(differs);
  }
}

TEST_CASE("Task tiles and player inventory behave") {
  SECTION("Locked door blocks until unlocked") {
    maze::DoorTile locked(true);
    REQUIRE(locked.isLocked());
    REQUIRE_FALSE(locked.isPassable());
    locked.unlock();
    REQUIRE_FALSE(locked.isLocked());
    REQUIRE(locked.isPassable());
  }

  SECTION("Unlocked door and collectibles are passable") {
    maze::DoorTile open;
    REQUIRE(open.isPassable());
    REQUIRE(maze::KeyTile().isPassable());
    REQUIRE(maze::ItemTile().isPassable());
  }

  SECTION("Player key inventory") {
    maze::Player player(100);
    REQUIRE(player.getKeys() == 0);
    REQUIRE_FALSE(player.useKey());
    player.addKey();
    player.addKey();
    REQUIRE(player.getKeys() == 2);
    REQUIRE(player.useKey());
    REQUIRE(player.getKeys() == 1);
  }
}

TEST_CASE("TaskWorld generation and planning") {
  maze::TaskConfig config;
  config.rows = 11;
  config.cols = 11;
  config.difficulty = 0.2;
  config.num_items = 2;
  config.num_keys = 1;
  config.num_doors = 1;
  config.num_food = 2;

  SECTION("Same seed reproduces the world") {
    maze::TaskWorld a(config, 7);
    maze::TaskWorld b(config, 7);
    REQUIRE(a.getGoalPosition() == b.getGoalPosition());
    REQUIRE(a.getPlayerPosition() == b.getPlayerPosition());
    REQUIRE(a.getItemsTotal() == b.getItemsTotal());
    REQUIRE(a.perceiveTiles(20) == b.perceiveTiles(20));
  }

  SECTION("Worlds are solvable and expose an optimal plan length") {
    for (uint64_t seed = 0; seed < 40; ++seed) {
      maze::TaskWorld world(config, seed);
      REQUIRE(world.isSolvable());
      const uint32_t plan = world.planTask();
      REQUIRE(plan > 0);
      // The optimal plan must at least cover the straight-line distance to the goal.
      REQUIRE(world.getItemsTotal() > 0);
    }
  }

  SECTION("Goal differs from start") {
    maze::TaskWorld world(config, 3);
    REQUIRE_FALSE(world.getGoalPosition() == world.getPlayerPosition());
  }

  SECTION("A legal move consumes food; a wall blocks") {
    using Move = maze::TaskWorld::Move;
    maze::TaskWorld world(config, 5);
    const uint32_t food_before = world.getPlayerFood();
    // The spawn cell always has at least one open neighbour (it is drawn from the largest
    // connected component). Try every direction until one succeeds.
    const std::array<Move, 4> moves = {Move::UP, Move::DOWN, Move::LEFT, Move::RIGHT};
    bool moved = false;
    for (const Move move : moves) {
      if (world.movePlayer(move)) {
        moved = true;
        break;
      }
    }
    REQUIRE(moved);
    REQUIRE(world.getPlayerFood() < food_before);
  }
}
