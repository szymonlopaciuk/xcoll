// copyright ############################### #
// This file is part of the Xcoll Package.   #
// Copyright (c) CERN, 2024.                 #
// ######################################### #

#ifndef XCOLL_COLL_GEOM_H
#define XCOLL_COLL_GEOM_H
#include <math.h>
#include <stdio.h>

typedef struct CollimatorGeometry_ {
    // Collimator jaws (with tilts)
    double jaw_LU;
    double jaw_LD;
    double jaw_RU;
    double jaw_RD;
    // TODO: need shortening of active length!
    double length;
    int8_t side  ;
    // Get angles of jaws
    double sin_zL;
    double cos_zL;
    double sin_zR;
    double cos_zR;
    double sin_zDiff;
    double cos_zDiff;
    int8_t jaws_parallel;
    // Tilts
    double sin_yL;
    double cos_yL;
    double tan_yL;
    double sin_yR;
    double cos_yR;
    double tan_yR;
    // Impact table
    InteractionRecordData record;
    RecordIndex record_index;
    int8_t record_touches;
} CollimatorGeometry_;
typedef CollimatorGeometry_ *CollimatorGeometry;


// This function checks if a particle hits a jaw (and which).
// Return value: 0 (no hit), 1 (hit on left jaw), -1 (hit on right jaw).
// Furthermore, the particle is moved to the location where it hits the jaw (drifted to the end if no hit),
//              and transformed to the reference frame of that jaw.
/*gpufun*/
int8_t hit_jaws_check_and_transform(LocalParticle* part, CollimatorGeometry cg){
    double part_x, part_tan;
    double jaw_s[2];
    double jaw_x[2];
    int8_t is_hit = 0;
    double s_L = 1.e21;
    double s_R = 1.e21;

    // Find the first hit on the left jaw (if not a right-sided collimator)
    if (cg->side != -1){
        SRotation_single_particle(part, cg->sin_zL, cg->cos_zL);
        part_x = LocalParticle_get_x(part);
#ifdef XTRACK_USE_EXACT_DRIFTS
        part_tan = LocalParticle_get_exact_xp(part);
#else
        part_tan = LocalParticle_get_xp(part);
#endif
        jaw_x[0] = cg->jaw_LU;
        jaw_x[1] = cg->jaw_LD;
        jaw_s[0] = cg->length/2*(1-cg->cos_yL);
        jaw_s[1] = cg->length/2*(1+cg->cos_yL);
        s_L = get_s_of_first_crossing_with_open_polygon(part_x, part_tan, jaw_s, jaw_x, 2, cg->tan_yL, 1);
        if (s_L < 1.e20){
            is_hit = 1;
        } else if (cg->side == 1){
            // If left-sided and no hit, rotate back to lab frame
            SRotation_single_particle(part, -cg->sin_zL, cg->cos_zL);
        }
    }

    // if rightsided:            lab  frame
    // if leftsided  and no hit: lab  frame
    // if leftsided  and hit:    left frame
    // if bothsided  and no hit: left frame
    // if bothsided  and hit:    left frame

    // Find the first hit on the right jaw (if not a left-sided collimator)
    if (cg->side != 1){
        if (cg->side == -1){
            // We didn't rotate to the left frame earlier, so full rotation to the right frame now
            SRotation_single_particle(part, cg->sin_zR, cg->cos_zR);
            part_x = LocalParticle_get_x(part);
#ifdef XTRACK_USE_EXACT_DRIFTS
            part_tan = LocalParticle_get_exact_xp(part);
#else
            part_tan = LocalParticle_get_xp(part);
#endif
        } else if (!cg->jaws_parallel){
            // We rotated to the left frame before, so now rotate the difference
            SRotation_single_particle(part, cg->sin_zDiff, cg->cos_zDiff);
            part_x = LocalParticle_get_x(part);
#ifdef XTRACK_USE_EXACT_DRIFTS
            part_tan = LocalParticle_get_exact_xp(part);
#else
            part_tan = LocalParticle_get_xp(part);
#endif
        }
        jaw_x[0] = cg->jaw_RU;
        jaw_x[1] = cg->jaw_RD;
        jaw_s[0] = cg->length/2*(1-cg->cos_yR);
        jaw_s[1] = cg->length/2*(1+cg->cos_yR);
        s_R = get_s_of_first_crossing_with_open_polygon(part_x, part_tan, jaw_s, jaw_x, 2, cg->tan_yR, -1);
        if (s_R < 1.e20 && s_R < s_L){
            is_hit = -1;
        } else if (is_hit == 1){
            if (!cg->jaws_parallel){
                // Rotate back to left frame
                SRotation_single_particle(part, -cg->sin_zDiff, cg->cos_zDiff);
            }
        } else {
            // No hit, rotate back to lab frame
            SRotation_single_particle(part, -cg->sin_zR, cg->cos_zR);
        }
    }

    // if rightsided and no hit: lab   frame
    // if rightsided and hit:    right frame
    // if leftsided  and no hit: lab   frame
    // if leftsided  and hit:    left  frame
    // if bothsided  and no hit: lab  frame
    // if bothsided  and hit:    hit   frame


    // Drift to the impact position or end, and move to jaw frame if relevant
    if (is_hit == 1){
        // Move to the impact position
        Drift_single_particle(part, s_L);
        // Shift the reference frame to the jaw corner LU
        XYShift_single_particle(part, cg->jaw_LU, 0);
        LocalParticle_add_to_s(part, -cg->length/2*(1 - cg->cos_yL));
        // Rotate the reference frame to tilt
        double new_s = YRotation_single_particle_rotate_only(part, LocalParticle_get_s(part), asin(cg->sin_yL));
        LocalParticle_set_s(part, new_s);
        if (cg->record_touches){
            InteractionRecordData_log(cg->record, cg->record_index, part, XC_ENTER_JAW_L);
        }

    } else if (is_hit == -1){
        // Move to the impact position
        Drift_single_particle(part, s_R);
        // Shift the reference frame to the jaw corner RU
        XYShift_single_particle(part, cg->jaw_RU, 0);
        LocalParticle_add_to_s(part, -cg->length/2*(1 - cg->cos_yR));
        // Rotate the reference frame to tilt
        double new_s = YRotation_single_particle_rotate_only(part, LocalParticle_get_s(part), asin(cg->sin_yR));
        LocalParticle_set_s(part, new_s);
        // Mirror x
        LocalParticle_scale_x(part, -1);
        LocalParticle_scale_px(part, -1);
        if (cg->record_touches){
            InteractionRecordData_log(cg->record, cg->record_index, part, XC_ENTER_JAW_R);
        }

    } else {
        Drift_single_particle(part, cg->length);
    }

    return is_hit;
}


/*gpufun*/
void hit_jaws_transform_back(int8_t is_hit, LocalParticle* part, CollimatorGeometry cg){
    if (is_hit == 1){
        // Rotate back from tilt
        double new_s = YRotation_single_particle_rotate_only(part, LocalParticle_get_s(part), -asin(cg->sin_yL));
        LocalParticle_set_s(part, new_s);
        // Shift the reference frame back from jaw corner LU
        XYShift_single_particle(part, -cg->jaw_LU, 0);
        LocalParticle_add_to_s(part, cg->length/2*(1 - cg->cos_yL));
        // If particle survived, drift to end of element
        if (LocalParticle_get_state(part) > 0){
            Drift_single_particle(part, cg->length - LocalParticle_get_s(part));
        }
        SRotation_single_particle(part, -cg->sin_zL, cg->cos_zL);

    } else if (is_hit == -1){
        // Mirror back
        LocalParticle_scale_x(part, -1);
        LocalParticle_scale_px(part, -1);
        // Rotate back from tilt
        double new_s = YRotation_single_particle_rotate_only(part, LocalParticle_get_s(part), -asin(cg->sin_yR));
        LocalParticle_set_s(part, new_s);
        // Shift the reference frame back from jaw corner RU
        XYShift_single_particle(part, -cg->jaw_RU, 0);
        LocalParticle_add_to_s(part, cg->length/2*(1 - cg->cos_yR));
        // If particle survived, drift to end of element
        if (LocalParticle_get_state(part) > 0){
            Drift_single_particle(part, cg->length - LocalParticle_get_s(part));
        }
        SRotation_single_particle(part, -cg->sin_zR, cg->cos_zR);
    }
}


#endif /* XCOLL_COLL_GEOM_H */
