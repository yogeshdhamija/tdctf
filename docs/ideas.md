- Add "pathing wobble" (creeps randomly choose among ties in the shortest path) to add an element of chance
- Add possible "critical hit chance" and "critical hit damage" features to towers (to happen instead of the usual damage) to add an element of chance
- Strictly limit planning time to 60s per player turn to prevent excessive computation
 
- Fog of war design changes:
  - Most level 1 towers are instant-build (therefore visible), but weak to encourage bluffing (I built an electric tower, you better build rubber creeps!)
  - Tower upgrades cost more turns to research, but are invisible to enemy until completed (or scouted by specialty creeps)
  - Most level 1 creep upgrades are instant-build (therefore visible), but weak
  - Creep upgrades cost more turns to research, but are invisible to enemy until completed (and cannot be scouted)
   
- Tower upgrades and creep upgrades are cancellable for a refund (refund is specified in the config file, can be unique per tower level and per creep upgrade). Also, refund can be different depending on how many turns of research have been completed. This is to allow pivoting strategies.
