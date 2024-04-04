internal sim_entity_hash *GetHashFromStorageIndex(sim_region *SimRegion, uint32 StorageIndex) {
  Assert(StorageIndex);

  sim_entity_hash *Result = 0;

  uint32 HashValue = StorageIndex;
  for(uint32 Offset = 0; Offset < ArrayCount(SimRegion->Hash); ++Offset) {
    // STUDY: WTF This does? Aparently when we do not get the value only by the hashValue we need to search one by one (offset)
    // starting from the hashValue index, but when we get to the end of the array, if not found yet we need to wrap back to the beginning of the array
    // and the second part does that (ArrayCount(SimRegion->Hash) - 1)
    uint32 HashMask = (ArrayCount(SimRegion->Hash) - 1);
    uint32 HashIndex = ((HashValue + Offset) & HashMask);
    sim_entity_hash *Entry = SimRegion->Hash + HashIndex;

    if (Entry->Index == 0 || Entry->Index == StorageIndex) {
      Result = Entry;
      break;
    }
  }

  return Result;
}

inline sim_entity *GetEntityByStorageIndex(sim_region *SimRegion, uint32 StorageIndex) {
  sim_entity_hash *Entry = GetHashFromStorageIndex(SimRegion, StorageIndex);
  sim_entity *Result = Entry->Ptr;
  return Result;
}

inline v2 GetSimSpaceP(sim_region *SimRegion, low_entity *Stored) {
  // NOTE: Map the entity into camera space
  // TODO: Do we want to set this to signaling NAN in
  // debug mode to make sure nobody ever uses the position
  // of a nonspatial entity?
  v2 Result = InvalidP;
  if (!IsSet(&Stored->Sim, EntityFlag_Nonspatial)) {
    world_difference Diff = Subtract(SimRegion->World, &Stored->P, &SimRegion->Origin);
    Result = Diff.dXY;
  }

  return Result;
}

internal sim_entity *AddEntity(game_state *GameState, sim_region *SimRegion, uint32 StorageIndex, low_entity *Source, v2 *SimP);
inline void LoadEntityReference(game_state *GameState, sim_region *SimRegion, entity_reference *Ref) {
  if (Ref->Index) {
    sim_entity_hash *Entry = GetHashFromStorageIndex(SimRegion, Ref->Index);
    if (Entry->Ptr == 0) {
      Entry->Index = Ref->Index;
      low_entity *LowEntity = GetLowEntity(GameState, Ref->Index);
      v2 P = GetSimSpaceP(SimRegion, LowEntity);
      Entry->Ptr = AddEntity(GameState, SimRegion, Ref->Index, LowEntity, &P);
    }

    Ref->Ptr = Entry->Ptr;
  }
}

inline void StoreEntityReference(entity_reference *Ref) {
  if (Ref->Ptr != 0) {
    Ref->Index = Ref->Ptr->StorageIndex;
  }
}

internal sim_entity *AddEntityRaw(game_state *GameState, sim_region *SimRegion, uint32 StorageIndex, low_entity *Source) {
  Assert(StorageIndex);
  sim_entity *Entity = 0;

  sim_entity_hash *Entry = GetHashFromStorageIndex(SimRegion, StorageIndex);
  if (Entry->Ptr == 0) {
    if (SimRegion->EntityCount < SimRegion->MaxEntityCount) {
      Entity = SimRegion->Entities + SimRegion->EntityCount++;

      Entry->Index = StorageIndex;
      Entry->Ptr = Entity;

      if (Source) {
        // TODO: This should really be a decompression step, not a copy!
        *Entity = Source->Sim;
        LoadEntityReference(GameState, SimRegion, &Entity->Sword);

        Assert(!IsSet(&Source->Sim, EntityFlag_Simming));
        AddFlag(&Source->Sim, EntityFlag_Simming);
      }

      Entity->StorageIndex = StorageIndex;
      Entity->Updatable = false;
    } else {
      InvalidCodePath;
    }
  }

  return Entity;
}

internal sim_entity *AddEntity(game_state *GameState, sim_region *SimRegion, uint32 StorageIndex, low_entity *Source, v2 *SimP) {
  sim_entity *Dest = AddEntityRaw(GameState, SimRegion, StorageIndex, Source);
  if (Dest) {
    if (SimP) {
      Dest->P = *SimP;
      Dest->Updatable = IsInRectangle(SimRegion->UpdatableBounds, Dest->P);
    } else {
      Dest->P = GetSimSpaceP(SimRegion, Source);
    }
  }

  return Dest;
}

internal sim_region *BeginSim(memory_arena *SimArena, game_state *GameState, world *World, world_position Origin, rectangle2 Bounds) {
  // TODO: If entities were stored in the world, we wouldnt need the game state here!

  sim_region *SimRegion = PushStruct(SimArena, sim_region);
  ZeroStruct(SimRegion->Hash);

  // TODO: IMPORTANT: Calculate this eventually from the maximum value of all entities radius plus their speed!
  real32 UpdateSafetyMargin = 1.0f;

  SimRegion->World = World;
  SimRegion->Origin = Origin;
  SimRegion->UpdatableBounds = Bounds;
  SimRegion->Bounds = AddRadiusTo(SimRegion->UpdatableBounds, UpdateSafetyMargin, UpdateSafetyMargin);

  // TODO: Need to be more specific about entity counts
  SimRegion->MaxEntityCount = 4096;
  SimRegion->EntityCount = 0;
  SimRegion->Entities = PushArray(SimArena, SimRegion->MaxEntityCount, sim_entity);

  world_position MinChunkP = MapIntoChunkSpace(World, SimRegion->Origin, GetMinCorner(SimRegion->Bounds));
  world_position MaxChunkP = MapIntoChunkSpace(World, SimRegion->Origin, GetMaxCorner(SimRegion->Bounds));
  for(int32 ChunkY = MinChunkP.ChunkY; ChunkY <= MaxChunkP.ChunkY; ++ChunkY) {
    for(int32 ChunkX = MinChunkP.ChunkX; ChunkX <= MaxChunkP.ChunkX; ++ChunkX) {
      world_chunk *Chunk = GetWorldChunk(World, ChunkX, ChunkY, SimRegion->Origin.ChunkZ);

      if (Chunk) {
        for(world_entity_block *Block = &Chunk->FirstBlock; Block; Block = Block->Next) {
          for (uint32 EntityIndex = 0; EntityIndex < Block->EntityCount; ++EntityIndex) {
            uint32 LowEntityIndex = Block->LowEntityIndex[EntityIndex];
            low_entity *Low = GameState->LowEntities + LowEntityIndex;

            if (!IsSet(&Low->Sim, EntityFlag_Nonspatial)) {
              v2 SimSpaceP = GetSimSpaceP(SimRegion, Low); // gets where the low is relative to the sim region
              if (IsInRectangle(SimRegion->Bounds, SimSpaceP)) {
                AddEntity(GameState, SimRegion, LowEntityIndex, Low, &SimSpaceP);
              }
            }
          }
        }
      }
    }
  }

  return SimRegion;
}

internal void EndSim(sim_region *Region, game_state *GameState) {
  // TODO: Maybe dont take a game state here, low entities should be stored in the world?
  sim_entity *Entity = Region->Entities;

  for(uint32 EntityIndex = 0; EntityIndex < Region->EntityCount; ++EntityIndex, ++Entity) {
    low_entity *Stored = GameState->LowEntities + Entity->StorageIndex;

    Assert(IsSet(&Stored->Sim, EntityFlag_Simming));
    Stored->Sim = *Entity;
    Assert(!IsSet(&Stored->Sim, EntityFlag_Simming));

    StoreEntityReference(&Stored->Sim.Sword);

    // TODO: Save state back to the stored entity, once high entities do state decompression, etc.

    world_position NewP = IsSet(Entity, EntityFlag_Nonspatial) ? NullPosition() : MapIntoChunkSpace(GameState->World, Region->Origin, Entity->P);
    ChangeEntityLocation(&GameState->WorldArena, GameState->World, Entity->StorageIndex, Stored, NewP);

    if (Entity->StorageIndex == GameState->CameraFollowingEntityIndex) {
      world_position NewCameraP = GameState->CameraP;

      NewCameraP.ChunkZ = Stored->P.ChunkZ;

#if 0
      if (CameraFollowingEntity.High->P.X > (9.0f * TileMap->TileSideInMeters)) {
        NewCameraP.AbsTileX += 17;
      }
      if (CameraFollowingEntity.High->P.X < -(9.0f * TileMap->TileSideInMeters)) {
        NewCameraP.AbsTileX -= 17;
      }
      // IMPORTANT: In the tutorial he puts 5f but 4.5f is better
      if (CameraFollowingEntity.High->P.Y > (4.5f * TileMap->TileSideInMeters)) {
        NewCameraP.AbsTileY += 9;
      }
      if (CameraFollowingEntity.High->P.Y < -(4.5f * TileMap->TileSideInMeters)) {
        NewCameraP.AbsTileY -= 9;
      }
#else
    // IMPORTANT: This centers camera on player and add smooth scroll
    NewCameraP = Stored->P;
#endif

    GameState->CameraP = NewCameraP;
    }
  }
}

internal bool32 TestWall(real32 WallX, real32 RelX, real32 RelY, real32 PlayerDeltaX, real32 PlayerDeltaY, real32 *tMin, real32 MinY, real32 MaxY) {
  bool32 Hit = false;
  real32 tEpsilon = 0.001f;

  if (PlayerDeltaX != 0) {
    // ts = (wx - p0x) / dx
    real32 tResult = (WallX - RelX) / PlayerDeltaX;
    real32 Y = RelY + tResult * PlayerDeltaY;

    if ((tResult >= 0.0f) && (*tMin > tResult)) {
      if ((Y >= MinY) && (Y <= MaxY)) {
        *tMin = Maximum(0.0f, tResult - tEpsilon);
        Hit = true;
      }
    }
  }

  return Hit;
}

internal void HandleCollision(sim_entity *A, sim_entity *B) {
  if ((A->Type == EntityType_Monster) && (B->Type == EntityType_Sword)) {
    --A->HitPointMax;
    MakeEntityNonSpatial(B);
  }
}

internal void MoveEntity(sim_region *SimRegion, sim_entity *Entity, real32 dt, move_spec *MoveSpec, v2 ddP) {
  Assert(!IsSet(Entity, EntityFlag_Nonspatial));

  world *World = SimRegion->World;

  if (MoveSpec->UnitMaxAccelVector) {
    real32 ddPLenght = LengthSq(ddP);
    if (ddPLenght > 1.0f) {
      // a1x = |a1|ax / |a| => change length of vector without changing its direction
      ddP *= (1.0f / SquareRoot(ddPLenght));
    }
  }

  ddP *= MoveSpec->Speed; // m/s^2

  // IMPORTANT: total hack, we need Ordinary Differential Equations to properly set friction
  ddP += -MoveSpec->Drag * Entity->dP;

  v2 OldPlayerP = Entity->P;
  v2 PlayerDelta = 0.5f * ddP * Square(dt) + Entity->dP * dt; // delta between player position and the position it will be if no collision occurs

  Entity->dP = ddP * dt + Entity->dP; // P'' (velocity) = at + v
  v2 NewPlayerP = OldPlayerP + PlayerDelta;

  real32 ddZ = -9.8f;
  Entity->Z = 0.5f * ddZ * Square(dt) + Entity->dZ * dt + Entity->Z;
  Entity->dZ = ddZ*dt + Entity->dZ;

  if (Entity->Z < 0) {
    Entity->Z = 0;
  }

  /**
   *
   * Collision detection
   *
   */
  real32 DistanceRemaining = Entity->DistanceLimit;
  if (DistanceRemaining == 0.0f) {
    // TODO: Do we want to formalize this number?
    DistanceRemaining = 10000.0f;
  }

  for (uint32 Iteration = 0; Iteration < 4; ++Iteration) {
    real32 tMin = 1.0f;
    real32 PlayerDeltaLength = Length(PlayerDelta);
    // TODO: What do we want to do for epsilons here?
    // Think this through for the final collision code
    if (PlayerDeltaLength > 0.0f) {

      if (PlayerDeltaLength > DistanceRemaining) {
        tMin = (DistanceRemaining / PlayerDeltaLength);
      }


      v2 WallNormal = {}; // r -> normal vector of the surface (new Y coordinate relative to wall (new x))
      sim_entity *HitEntity = 0;

      v2 DesiredPosition = Entity->P + PlayerDelta;

      bool32 StopsOnCollision = IsSet(Entity, EntityFlag_Collides);

      if(!IsSet(Entity, EntityFlag_Nonspatial)) {
        // TODO: Spatial partition here!
        for(uint32 TestHighEntityIndex = 0; TestHighEntityIndex < SimRegion->EntityCount; ++TestHighEntityIndex) { // begins on 1 to ignore null entity
          sim_entity *TestEntity = SimRegion->Entities + TestHighEntityIndex;
          if (Entity != TestEntity) {
            if (IsSet(TestEntity, EntityFlag_Collides) && !IsSet(TestEntity, EntityFlag_Nonspatial)) {
              real32 DiameterW = TestEntity->Width + Entity->Width;
              real32 DiameterH = TestEntity->Height + Entity->Height;

              v2 MinCorner = -0.5f * V2(DiameterW, DiameterH);
              v2 MaxCorner = 0.5f * V2(DiameterW, DiameterH);

              v2 Rel = Entity->P - TestEntity->P;

              if (TestWall(MinCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin, MinCorner.Y, MaxCorner.Y)) {
                WallNormal = V2(-1, 0);
                HitEntity = TestEntity;
              }
              if (TestWall(MaxCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin, MinCorner.Y, MaxCorner.Y)) {
                WallNormal = V2(1, 0);
                HitEntity = TestEntity;
              }
              if (TestWall(MinCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin, MinCorner.X, MaxCorner.X)) {
                WallNormal = V2(0, -1);
                HitEntity = TestEntity;
              }
              if (TestWall(MaxCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin, MinCorner.X, MaxCorner.X)) {
                WallNormal = V2(0, 1);
                HitEntity = TestEntity;
              }
            }
          }
        }
      }

      Entity->P += tMin * PlayerDelta;
      DistanceRemaining -= tMin * PlayerDeltaLength;
      if (HitEntity) {
        PlayerDelta = DesiredPosition - Entity->P;
        if (StopsOnCollision) {
          Entity->dP = Entity->dP - 1 * Inner(Entity->dP, WallNormal) * WallNormal; // V' = V - 2 * vTr * r => to bounce on wall colision ( use -1 to slide on wall)
          PlayerDelta = PlayerDelta - 1 * Inner(PlayerDelta, WallNormal) * WallNormal;
        }
        // TODO: IMPORTANT: Need our collision table here!!!

        sim_entity *A = Entity;
        sim_entity *B = HitEntity;
        if (A->Type > B->Type) {
          sim_entity *Temp = B;
          B = A;
          A = Temp;
        }

        HandleCollision(A, B);

        // TODO: Stairs
        // Entity->AbsTileZ += HitLow->dAbsTileZ;
      } else {
        break;
      }
    } else {
      break;
    }
  }

  if (Entity->DistanceLimit != 0.0f) {
    Entity->DistanceLimit = DistanceRemaining;
  }

  // TODO: Change to using the acceleration vector
  if ((Entity->dP.X == 0.0f) && (Entity->dP.Y == 0.0f)) {
    // NOTE: Leave FacingDirection whatever it was
  } else if (AbsoluteValue(Entity->dP.X) > AbsoluteValue(Entity->dP.Y)) {
    if (Entity->dP.X > 0) {
      Entity->FacingDirection = 0;
    } else {
      Entity->FacingDirection = 2;
    }
  } else {
    if (Entity->dP.Y > 0) {
      Entity->FacingDirection = 1;
    } else {
      Entity->FacingDirection = 3;
    }
  }
}
