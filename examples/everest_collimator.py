# copyright ############################### #
# This file is part of the Xcoll Package.   #
# Copyright (c) CERN, 2024.                 #
# ######################################### #

import numpy as np
from pathlib import Path
import sys, os, contextlib
import matplotlib.pyplot as plt

import xobjects as xo
import xtrack as xt
import xpart as xp
import xcoll as xc



# --------------------------------------------------------
# -------------------- Initialisation --------------------
# --------------------------------------------------------
# Make a context and get a buffer
context = xo.ContextCpu()         # For CPU
# context = xo.ContextCupy()      # For CUDA GPUs
# context = xo.ContextPyopencl()  # For OpenCL GPUs

beam = 1
path_in  = xc._pkg_root.parent / 'examples'


# Load from json
with open(os.devnull, 'w') as fid:
    with contextlib.redirect_stdout(fid):
        line = xt.Line.from_json(path_in / 'machines' / f'lhc_run3_b{beam}.json')


# Initialise collmanager
coll_manager = xc.CollimatorManager.from_yaml(path_in / 'colldb' / f'lhc_run3.yaml',
                                              line=line, beam=beam, _context=context)


# Install collimators into line
coll_manager.install_everest_collimators(verbose=True)


# Aperture model check
print('\nAperture model check after introducing collimators:')
with open(os.devnull, 'w') as fid:
    with contextlib.redirect_stdout(fid):
        df_with_coll = line.check_aperture()
assert not np.any(df_with_coll.has_aperture_problem)


# Build the tracker
coll_manager.build_tracker()


# Set the collimator openings based on the colldb,
# or manually override with the option gaps={collname: gap}
coll_manager.set_openings()


# --------------------------------------------------------
# ------------------ Tracking (test 1) -------------------
# --------------------------------------------------------
#
# As a first test, we just track 5 turns.
# We expect to see the transversal profile generated by
# the three primaries opened at 5 sigma.


# Create initial particles
n_sigmas = 10
n_part = 50000
x_norm = np.random.uniform(-n_sigmas, n_sigmas, n_part)
y_norm = np.random.uniform(-n_sigmas, n_sigmas, n_part)
part = line.build_particles(x_norm=x_norm, y_norm=y_norm,
                            nemitt_x=3.5e-6, nemitt_y=3.5e-6,
                            at_element='tcp.d6l7.b1',
                            match_at_s=coll_manager.s_active_front['tcp.d6l7.b1']
                           )

# Track
print("Tracking first test.. ")
coll_manager.enable_scattering()
line.track(part, num_turns=1)
coll_manager.disable_scattering()

# Sort the particles by their ID
part.sort(interleave_lost_particles=True)

# Plot the surviving particles as green
plt.figure(1,figsize=(12,12))
plt.plot(x_norm, y_norm, '.', color='red')
plt.plot(x_norm[part.state>0], y_norm[part.state>0], '.', color='green')
plt.axis('equal')
plt.axis([n_sigmas, -n_sigmas, -n_sigmas, n_sigmas])
plt.show()


# --------------------------------------------------------
# ------------------ Tracking (test 2) -------------------
# --------------------------------------------------------
#
# As a second test, we remove all collimator openings
# (which is done by setting them to 1 meter) except the
# horizontal primary. We give the latter an asymmetric
# opening, and an angle of 15 degrees.
# This is done to check our coordinate implementations.
# We only track one turn, because otherwise betatron
# oscillations would make the cut profile symmetric anyway.

coll_manager.colldb.angle = {'tcp.c6l7.b1': 15}
coll_manager.set_openings({'tcp.c6l7.b1': [4,7]}, full_open=True)

# Create initial particles
part = line.build_particles(x_norm=x_norm, y_norm=y_norm,
                            nemitt_x=3.5e-6, nemitt_y=3.5e-6,
                            at_element='tcp.c6l7.b1',
                            match_at_s=coll_manager.s_active_front['tcp.c6l7.b1']
                           )

# Track
print("Tracking second test.. ")
coll_manager.enable_scattering()
line.track(part, num_turns=1)
coll_manager.disable_scattering()

# Sort the particles by their ID
part.sort(interleave_lost_particles=True)

# Plot the surviving particles as green
plt.figure(1,figsize=(12,12))
plt.plot(x_norm, y_norm, '.', color='red', alpha=0.4)
plt.plot(x_norm[part.state>0], y_norm[part.state>0], '.', color='green')
plt.axis('equal')
plt.axis([n_sigmas, -n_sigmas, -n_sigmas, n_sigmas])
plt.show()

