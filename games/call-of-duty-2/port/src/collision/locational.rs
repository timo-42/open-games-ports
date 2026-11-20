//! G_TracePointEx uses SV_Trace with static geometry and a hit-location priority table.
use super::{CollisionMap, TraceResult, dobj};
use crate::{assets::models::ModelRegistry, vm::game_entities::GameEntities};
use std::io;
type Vec3 = [f32; 3];
impl CollisionMap {
    #[allow(clippy::too_many_arguments)]
    pub fn trace_point(
        &self,
        entities: &GameEntities,
        models: &ModelRegistry,
        start: Vec3,
        end: Vec3,
        pass: u16,
        mask: u32,
        rifle: impl Into<Option<bool>>,
    ) -> io::Result<TraceResult> {
        let rifle = rifle.into();
        let mut hit = self.trace(start, end, [0.; 3], [0.; 3], mask);
        if hit.fraction == 0. {
            return Ok(hit);
        }
        self.clip_static_models(start, end, mask, &mut hit)?;
        if hit.fraction == 0. {
            return Ok(hit);
        }
        for (slot, ent) in self.trace_candidates(entities, start, end, None, pass, mask) {
            if (rifle.is_some() || ent.sv_flags & 4 != 0)
                && dobj::clip_entity(
                    models,
                    slot,
                    ent,
                    start,
                    end,
                    mask,
                    rifle.unwrap_or(false),
                    &mut hit,
                )?
            {
                continue;
            }
            if ent.link.area_contents & mask == 0
                || !dobj::box_intersects(
                    start,
                    end,
                    [ent.link.absmin, ent.link.absmax],
                    hit.fraction,
                )
            {
                continue;
            }
            let Some(mut next) = self.trace_entity(ent, start, end, [0.; 3], [0.; 3], mask) else {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidData,
                    "missing entity collision model",
                ));
            };
            let start_solid = hit.start_solid || next.start_solid;
            let all_solid = hit.all_solid || next.all_solid;
            if next.fraction < hit.fraction {
                next.entity = slot;
                next.surface_flags = 0;
                next.contents = ent.contents as u32;
                next.material = None;
                hit = next;
            }
            hit.start_solid = start_solid;
            hit.all_solid = all_solid;
        }
        Ok(hit)
    }
}
