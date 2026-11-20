//! CM_BoxLeafnums / BoxOnPlaneSide for the SV_LinkEntity cluster list.
use super::{CollisionMap, Plane};

impl Plane {
    pub(super) fn box_sides(&self, mins: [f32; 3], maxs: [f32; 3]) -> u8 {
        if let Some(axis) = self.axis {
            if self.dist <= mins[axis] {
                return 1;
            }
            if maxs[axis] <= self.dist {
                return 2;
            }
            return 3;
        }
        // The original x87 dot products are rounded into float stack slots
        // before comparison. Corners are selected by normal sign.
        let mut high = 0_f64;
        let mut low = 0_f64;
        for axis in 0..3 {
            let n = f64::from(self.normal[axis]);
            let (a, b) = if n < 0.0 {
                (mins[axis], maxs[axis])
            } else {
                (maxs[axis], mins[axis])
            };
            high += n * f64::from(a);
            low += n * f64::from(b);
        }
        u8::from(self.dist <= high as f32) | (u8::from((low as f32) < self.dist) << 1)
    }
}

impl CollisionMap {
    /// The first 128 intersecting leaves are considered, keeping up to 16
    /// nonnegative cluster entries, including duplicates. The overflow value
    /// comes from the *last* nonempty leaf visited in the full traversal.
    pub(crate) fn box_clusters(&self, mins: [f32; 3], maxs: [f32; 3]) -> (Vec<i16>, i16) {
        let mut stack = vec![0_i16];
        let mut clusters = Vec::new();
        let mut leaves = 0;
        while let Some(child) = stack.pop() {
            if child < 0 {
                leaves += 1;
                let cluster = self.clusters[(!child) as usize];
                if cluster != -1 {
                    clusters.push(cluster);
                }
                if clusters.len() == 16 {
                    return (clusters, self.last_box_cluster(mins, maxs));
                }
                if leaves == 128 {
                    break;
                }
            } else {
                let node = &self.nodes[child as usize];
                let sides = self.planes[node.plane].box_sides(mins, maxs);
                if sides != 1 {
                    stack.push(node.children[1]);
                }
                if sides != 2 {
                    stack.push(node.children[0]);
                }
            }
        }
        (clusters, 0)
    }

    /// SV_BuildCachedEntityListForPoint checks the first 128 intersecting leaves directly.
    pub(crate) fn box_visible(&self, observer: i16, mins: [f32; 3], maxs: [f32; 3]) -> bool {
        let mut stack = vec![0_i16];
        let mut count = 0;
        while let Some(child) = stack.pop() {
            if child < 0 {
                if self.cluster_visible(observer, self.clusters[(!child) as usize]) {
                    return true;
                }
                count += 1;
                if count == 128 {
                    break;
                }
            } else {
                let node = &self.nodes[child as usize];
                let sides = self.planes[node.plane].box_sides(mins, maxs);
                if sides != 1 {
                    stack.push(node.children[1]);
                }
                if sides != 2 {
                    stack.push(node.children[0]);
                }
            }
        }
        false
    }

    fn last_box_cluster(&self, mins: [f32; 3], maxs: [f32; 3]) -> i16 {
        // Reverse traversal finds the final usable leaf without expanding a
        // shared BSP subtree repeatedly after the original leaf buffer fills.
        let mut stack = vec![0_i16];
        let mut seen = vec![false; self.nodes.len()];
        while let Some(child) = stack.pop() {
            if child < 0 {
                let cluster = self.clusters[(!child) as usize];
                if cluster != -1 {
                    return cluster;
                }
            } else if !std::mem::replace(&mut seen[child as usize], true) {
                let node = &self.nodes[child as usize];
                let sides = self.planes[node.plane].box_sides(mins, maxs);
                if sides != 2 {
                    stack.push(node.children[0]);
                }
                if sides != 1 {
                    stack.push(node.children[1]);
                }
            }
        }
        self.clusters[0]
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::collision::Node;
    #[test]
    fn axial_contact_and_overflow_follow_original_order() {
        let plane = Plane {
            normal: [1., 0., 0.],
            dist: 0.,
            axis: Some(0),
        };
        assert_eq!(plane.box_sides([0.; 3], [0.; 3]), 1);
        assert_eq!(plane.box_sides([-1.; 3], [0.; 3]), 2);
        let mut nodes: Vec<_> = (0..5)
            .map(|i| Node {
                plane: 0,
                children: [i + 1, i + 1],
            })
            .collect();
        nodes.push(Node {
            plane: 0,
            children: [-1, -2],
        });
        let plane_bytes: Vec<_> = plane
            .normal
            .into_iter()
            .chain([plane.dist])
            .flat_map(f32::to_le_bytes)
            .collect();
        let mut node_bytes = Vec::new();
        for node in nodes {
            let mut bytes = [0_u8; 36];
            for (i, v) in [
                node.plane as i32,
                node.children[0] as i32,
                node.children[1] as i32,
            ]
            .into_iter()
            .enumerate()
            {
                bytes[i * 4..i * 4 + 4].copy_from_slice(&v.to_le_bytes());
            }
            node_bytes.extend_from_slice(&bytes);
        }
        let mut leaves = [0_u8; 72];
        leaves[..4].copy_from_slice(&21_i32.to_le_bytes());
        leaves[36..40].copy_from_slice(&64_i32.to_le_bytes());
        let map =
            CollisionMap::decode(&plane_bytes, &node_bytes, &leaves, &[]).expect("synthetic BSP");
        let (list, last) = map.box_clusters([-1.; 3], [1.; 3]);
        assert_eq!(list, [21, 64].repeat(8));
        assert_eq!(last, 64);
        assert_eq!(map.box_clusters([1.; 3], [2.; 3]), (vec![21], 0));
    }

    #[test]
    fn script_visibility_and_solid_keep_independent_masks_and_membership() -> std::io::Result<()> {
        use crate::vm::{ScriptWorld, entity_methods::dispatch, value::Value};
        let map = CollisionMap::decode(
            &[0; 16],
            &[0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255]
                .into_iter()
                .chain([0; 24])
                .collect::<Vec<u8>>(),
            &[0; 36],
            &[],
        )?;
        let mut world = ScriptWorld::default();
        let (_, object) = world.spawn_entity(b"script_model", [2., 3., 4.], 0)?;
        let client = world.attach_player(40, 0)?;
        let (_, ent) = world.entities.for_object_mut(object).expect("map entity");
        ent.flags = 0x1000;
        for (method, args) in [
            ("hide", vec![]),
            ("showtoplayer", vec![Value::Object(client)]),
            ("solid", vec![]),
        ] {
            dispatch(object, method, &args, &mut world, &map).expect("entity method")?;
        }
        let (_, ent) = world.entities.for_object_mut(object).expect("map entity");
        assert_eq!(ent.flags, 0x1000);
        assert_eq!(ent.client_mask, [u32::MAX, u32::MAX ^ 256]);
        assert!(ent.linked && ent.link.in_collision_world);
        assert_eq!(ent.contents, 0x2080);
        assert_eq!(ent.link.area_mins, [1., 2.]);
        assert_eq!(
            dispatch(object, "setcontents", &[Value::Int(0)], &mut world, &map).expect("method")?,
            Value::Int(0x2080)
        );
        let (_, ent) = world.entities.for_object_mut(object).expect("map entity");
        assert!(ent.linked);
        assert!(!ent.link.in_collision_world);
        assert!(
            dispatch(
                object,
                "showtoplayer",
                &[Value::Object(object)],
                &mut world,
                &map
            )
            .expect("method")
            .is_err()
        );
        Ok(())
    }
}
