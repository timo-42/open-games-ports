//! SV_PointContents (0x0809e368).
use super::{CollisionMap, dobj};
use crate::vm::game_entities::{GameEntities, touch::area_entities};
type Vec3 = [f32; 3];

impl CollisionMap {
    pub fn point_contents(
        &self,
        entities: &GameEntities,
        point: Vec3,
        pass: u16,
        mask: u32,
    ) -> u32 {
        let mut contents = self.model_point_contents(0, point);
        for slot in area_entities(entities, self, point, point, mask) {
            if slot == pass {
                continue;
            }
            let Some(entity) = entities.get(slot) else {
                continue;
            };
            // The temporary clip handle has an empty brush leaf: its lower
            // limits are FLT_MAX and its upper limits are -FLT_MAX. The server
            // point query can therefore receive entity contents only from an
            // inline brush model, even when a temporary capsule surrounds p.
            if !entity.bmodel {
                continue;
            }
            let local = if entity.angles == [0.; 3] {
                std::array::from_fn(|i| point[i] - entity.origin[i])
            } else {
                let [front, right, up] = crate::vm::math::axes(entity.angles);
                dobj::local(point, entity.origin, [front, right.map(|v| -v), up])
            };
            contents |= self.model_point_contents(entity.brush_model, local);
        }
        contents & mask
    }
}
