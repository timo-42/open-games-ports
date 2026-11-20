//! The seven gentity attachment slots. G_UpdateClientAttachments mirrors the first six.
use super::super::{GameHost, game_entities::MapEntity, invalid, precache, value::Value};
use crate::animation::pose::{BoneControl, PoseModel, PoseNode, evaluate};
use crate::assets::models::{MODEL_CONFIGSTRING_BASE, ModelRegistry};
use crate::collision::dobj::{DObjPose, PosedModel};
use crate::{level::ConfigStrings, snapshot::ClientState};
use std::io;

#[derive(Clone, Debug, Default)]
pub struct ModelAttachment {
    pub model_index: u8,
    /// None is a released string handle. Some("") requests the original named-bone meld.
    pub tag: Option<String>,
    pub ignore_collision: bool,
}

pub fn build_pose(
    registry: &ModelRegistry,
    definitions: &[(u8, Option<&str>, bool)],
    tree: &PoseNode<'_>,
    controls: &[BoneControl],
) -> io::Result<DObjPose> {
    let inputs = definitions
        .iter()
        .map(|&(id, tag, _)| {
            let model = registry
                .get(id)
                .ok_or_else(|| invalid("missing registered attachment model"))?;
            Ok(PoseModel {
                parts: &model.parts,
                parent_tag: tag.filter(|name| !name.is_empty()),
            })
        })
        .collect::<io::Result<Vec<_>>>()?;
    let matrices = evaluate(&inputs, tree, controls)?;
    Ok(assemble_pose(&inputs, definitions, matrices))
}

pub(crate) fn assemble_pose(
    inputs: &[PoseModel<'_>],
    definitions: &[(u8, Option<&str>, bool)],
    matrices: Vec<crate::assets::models::BoneTransform>,
) -> DObjPose {
    let mut cursor = matrices.into_iter();
    let mut models = Vec::with_capacity(inputs.len());
    for (index, (input, &(model_index, _, ignore_collision))) in
        inputs.iter().zip(definitions).enumerate()
    {
        let parent_bone = input
            .parent_tag
            .and_then(|tag| {
                inputs[..index]
                    .iter()
                    .flat_map(|i| &i.parts.bones)
                    .position(|bone| bone.name == tag)
            })
            .map(|i| i as u8);
        models.push(PosedModel {
            model_index,
            parent_bone,
            ignore_collision,
            bones: cursor.by_ref().take(input.parts.bones.len()).collect(),
        });
    }
    DObjPose { models }
}

pub(crate) fn mirror_client(
    entity: &MapEntity,
    client: &mut ClientState,
    strings: &mut ConfigStrings,
) -> io::Result<()> {
    client.set_field("modelindex", u32::from(entity.model_index))?;
    for (i, attachment) in entity.attachments[..6].iter().enumerate() {
        let tag_index = if attachment.model_index == 0 {
            0
        } else {
            precache::index(
                strings,
                0x6e,
                32,
                attachment.tag.as_deref().unwrap_or_default().as_bytes(),
                true,
                "tag",
            )?
        };
        client.set_field(
            &format!("attachModelIndex[{i}]"),
            u32::from(attachment.model_index),
        )?;
        client.set_field(&format!("attachTagIndex[{i}]"), tag_index as u32)?;
    }
    Ok(())
}

type ModelBounds = [[f32; 3]; 2];

pub(super) fn geometry(
    registry: &ModelRegistry,
    base: u8,
    slots: &[ModelAttachment; 7],
) -> io::Result<(Option<ModelBounds>, Option<DObjPose>)> {
    if base == 0 {
        return Ok((None, None));
    }
    let mut bounds = registry
        .get(base)
        .ok_or_else(|| invalid("missing registered model"))?
        .bounds;
    let mut defs = vec![(base, None, false)];
    for slot in slots.iter().filter(|s| s.model_index != 0) {
        let extra = registry
            .get(slot.model_index)
            .ok_or_else(|| invalid("missing attachment model"))?;
        for (sum, add) in bounds
            .iter_mut()
            .flatten()
            .zip(extra.bounds.iter().flatten())
        {
            *sum += *add;
        }
        defs.push((slot.model_index, slot.tag.as_deref(), slot.ignore_collision));
    }
    let pose = if defs.len() > 1 {
        Some(build_pose(
            registry,
            &defs,
            &PoseNode::Group(Vec::new()),
            &[],
        )?)
    } else {
        None
    };
    Ok((Some(bounds), pose))
}

fn integer(args: &[Value], i: usize) -> io::Result<i32> {
    match args.get(i) {
        Some(Value::Int(n)) => Ok(*n),
        _ => Err(invalid(format!("parameter {} is not an integer", i + 1))),
    }
}

fn string(args: &[Value], i: usize) -> io::Result<String> {
    let bytes = args
        .get(i)
        .ok_or_else(|| invalid(format!("parameter {} does not exist", i + 1)))?
        .script_string()?;
    let bytes = bytes.split(|b| *b == 0).next().unwrap_or_default();
    std::str::from_utf8(bytes)
        .map(str::to_owned)
        .map_err(|_| invalid("invalid attachment name"))
}

pub(crate) fn dispatch(
    host: &mut GameHost<'_>,
    object: u32,
    name: &str,
    args: &[Value],
) -> Option<io::Result<Value>> {
    if !matches!(
        name,
        "attach"
            | "detach"
            | "detachall"
            | "getattachsize"
            | "getattachmodelname"
            | "getattachtagname"
            | "getattachignorecollision"
    ) {
        return None;
    }
    Some((|| {
        let (slot, entity) = host
            .world
            .entities
            .for_object_mut(object)
            .ok_or_else(|| invalid("not an entity"))?;
        let model_name = |id: u8| {
            host.strings
                .get(MODEL_CONFIGSTRING_BASE + usize::from(id))
                .expect("model configstring")
        };
        match name {
            "getattachsize" => {
                return Ok(Value::Int(
                    entity
                        .attachments
                        .iter()
                        .take_while(|a| a.model_index != 0)
                        .count() as i32,
                ));
            }
            "getattachmodelname" | "getattachtagname" | "getattachignorecollision" => {
                let a = entity
                    .attachments
                    .get(integer(args, 0)? as usize)
                    .filter(|a| a.model_index != 0)
                    .ok_or_else(|| invalid("bad index"))?;
                return Ok(match name {
                    "getattachmodelname" => Value::String(model_name(a.model_index).to_vec()),
                    "getattachtagname" => {
                        Value::String(a.tag.as_deref().unwrap_or_default().as_bytes().to_vec())
                    }
                    _ => Value::Int(i32::from(a.ignore_collision)),
                });
            }
            _ => {}
        }
        let mut next = entity.attachments.clone();
        let mut duplicate = None;
        if name == "detachall" {
            next = Default::default();
        } else {
            let model = string(args, 0)?;
            let tag = if args.len() >= 2 {
                string(args, 1)?.to_ascii_lowercase()
            } else {
                String::new()
            };
            let flag = name == "attach" && args.len() >= 3 && integer(args, 2)? != 0;
            if let Some(i) = next.iter().position(|a| {
                a.tag.as_deref() == Some(&tag)
                    && model_name(a.model_index).eq_ignore_ascii_case(model.as_bytes())
            }) {
                next[i..].rotate_left(1);
                next[6] = Default::default();
                if name == "attach" {
                    // The original removes an existing attachment before raising the duplicate error.
                    duplicate = Some(invalid(format!(
                        "model '{model}' already attached to tag '{tag}'"
                    )));
                }
            } else if name == "detach" {
                host.config.messages.push("Current attachments:\n".into());
                for a in next.iter().filter(|a| a.model_index != 0) {
                    host.config.messages.push(format!(
                        "model: '{}', tag: '{}'\n",
                        String::from_utf8_lossy(model_name(a.model_index)),
                        a.tag.as_deref().unwrap_or_default()
                    ));
                }
                return Err(invalid(format!(
                    "failed to detach model '{model}' from tag '{tag}'"
                )));
            } else {
                let free = next
                    .iter_mut()
                    .find(|a| a.model_index == 0)
                    .ok_or_else(|| invalid("maximum attached models exceeded"))?;
                free.model_index =
                    host.models
                        .index(host.assets, host.strings, &model, host.initializing)?;
                free.tag = Some(tag);
                free.ignore_collision |= flag;
            }
        }
        if slot >= 64 {
            let (bounds, pose) = geometry(host.models, entity.model_index, &next)?;
            entity.link.dobj_bounds = bounds;
            entity.dobj_pose = pose;
        }
        entity.attachments = next;
        if slot >= 64 {
            super::super::game_entities::tag_links::update_tags(host.world, slot, host.models)?;
        }
        if let Some(error) = duplicate {
            return Err(error);
        }
        Ok(Value::Undefined)
    })())
}
