#! /usr/bin/env python
# Copyright 2021 Neil Zaim
#
# This file is part of WarpX.
#
# License: BSD-3-Clause-LBNL

import os
import sys

import yt

sys.path.insert(1, '../../../../warpx/Regression/Checksum/')
import checksumAPI
import numpy as np
import scipy.constants as scc

## This script performs various checks for the proton boron nuclear fusion module. The simulation
## that we check is made of 5 different tests, each with different proton, boron and alpha species.
##
## The first test is performed in the proton-boron center of mass frame. It could correspond to the
## physical case of a proton beam colliding with a boron beam. The kinetic energy of the colliding
## particles depends on the cell number in the z direction and varies in the few keV to few MeV
## range. All the particles within a cell have the exact same momentum, which allows detailed
## checks of the energy of produced alpha particles. The proton and boron species have the same
## density and number of particles in this test. The number of produced alphas is much smaller than
## the initial number of protons and borons.
##
## The second test is performed in the boron rest frame. It corresponds to the physical case of a
## low density proton beam colliding with a high-density proton+boron target. The energy of the
## proton beam is varied in the few keV to few MeV range, depending on the cell number in the z
## direction. As in the previous case, all the particles within a cell have the exact same
## momentum, which allows detailed checks of the energy of produced alpha particles. In this test,
## there are 100 immobile boron and 100 immobile proton macroparticles per cell, as well as 900
## beam proton macroparticles per cell. The density of the immobile particles is 6 orders of
## magnitude higher than the number of beam particles, which means that they have a much higher
## weight. This test is similar to the example given in section 3 of Higginson et al.,
## Journal of Computation Physics, 388 439–453 (2019), which was found to be sensitive to the way
## unsampled pairs are accounted for. As before, the number of produced alphas is much smaller than
## the initial number of protons and borons.
##
## The third test corresponds to a Maxwellian plasma with a 44 keV temperature. The alpha yield is
## directly compared to the analytical fits of W.M. Nevins and R. Swain, Nuclear Fusion, 40, 865
## (2000) for a thermal plasma.
##
## The fourth test corresponds to a plasma with an extremely small boron density, so that all boron
## macroparticles should have disappeared by the end of the simulation, which we verify.
##
## The fifth test is exactly the same as the fourth test, except that the
## fusion_probability_threshold parameter is increased to an excessive value. Because of that, we
## severely underestimate the fusion yield and boron macroparticles remain at the end of the
## simulation, which we verify.
##
## In all simulations, we check particle number, charge, momentum and energy conservation and
## perform basic checks regarding the produced particles. When possible, we also compare the number
## of produced macroparticles, fusion yield and energy of the produced particles to theoretical
## values.
##
## Please be aware that the relative tolerances are often set empirically in this analysis script,
## so it would not be surprising that some tolerances need to be increased in the future.

default_tol = 1.e-12 # Default relative tolerance

## Define reactants and products
reactant_species = ['deuterium', 'tritium']
product_species = ['helium', 'neutron']

<<<<<<< HEAD
=======
from scipy.constants import m_p, m_u

>>>>>>> b11812de097d8c120b952460559af23a76258d5b
mass = {
    'deuterium': 2.01410177812*scc.m_u,
    'tritium': 3.0160492779*scc.m_u,
    'helium': 4.00260325413*scc.m_u,
    'neutron': 1.0013784193052508*scc.m_p
}
m_reduced = np.product([mass[s] for s in reactant_species])/np.sum([mass[s] for s in reactant_species])

## Some physical parameters
keV_to_Joule = scc.e*1e3
MeV_to_Joule = scc.e*1e6
barn_to_square_meter = 1.e-28

E_fusion = 17.6*MeV_to_Joule # Energy released during the fusion reaction

## Some numerical parameters for this test
size_x = 8
size_y = 8
size_z = 16
dV_total = size_x*size_y*size_z # Total simulation volume
# Volume of a slice corresponding to a single cell in the z direction. In tests 1 and 2, all the
# particles of a given species in the same slice have the exact same momentum
dV_slice = size_x*size_y
dt = 1./(scc.c*np.sqrt(3.))
# In test 1 and 2, the energy in cells number i (in z direction) is typically Energy_step * i**2
Energy_step = 22.*keV_to_Joule

def is_close(val1, val2, rtol=default_tol, atol=0.):
    ## Wrapper around numpy.isclose, used to override the default tolerances.
    return np.isclose(val1, val2, rtol=rtol, atol=atol)

def add_existing_species_to_dict(yt_ad, data_dict, species_name, prefix, suffix):
    data_dict[prefix+"_px_"+suffix]  = yt_ad[species_name, "particle_momentum_x"].v
    data_dict[prefix+"_py_"+suffix]  = yt_ad[species_name, "particle_momentum_y"].v
    data_dict[prefix+"_pz_"+suffix]  = yt_ad[species_name, "particle_momentum_z"].v
    data_dict[prefix+"_w_"+suffix]   = yt_ad[species_name, "particle_weight"].v
    data_dict[prefix+"_id_"+suffix]  = yt_ad[species_name, "particle_id"].v
    data_dict[prefix+"_cpu_"+suffix] = yt_ad[species_name, "particle_cpu"].v
    data_dict[prefix+"_z_"+suffix]   = yt_ad[species_name, "particle_position_z"].v

def add_empty_species_to_dict(data_dict, species_name, prefix, suffix):
    data_dict[prefix+"_px_"+suffix]  = np.empty(0)
    data_dict[prefix+"_py_"+suffix]  = np.empty(0)
    data_dict[prefix+"_pz_"+suffix]  = np.empty(0)
    data_dict[prefix+"_w_"+suffix]   = np.empty(0)
    data_dict[prefix+"_id_"+suffix]  = np.empty(0)
    data_dict[prefix+"_cpu_"+suffix] = np.empty(0)
    data_dict[prefix+"_z_"+suffix]   = np.empty(0)

def add_species_to_dict(yt_ad, data_dict, species_name, prefix, suffix):
    try:
        ## If species exist, we add its data to the dictionary
        add_existing_species_to_dict(yt_ad, data_dict, species_name, prefix, suffix)
    except yt.utilities.exceptions.YTFieldNotFound:
        ## If species does not exist, we avoid python crash and add empty arrays to the
        ## dictionnary. Currently, this happens for the boron species in test number 4, which
        ## entirely fuses into alphas.
        add_empty_species_to_dict(data_dict, species_name, prefix, suffix)

def check_particle_number_conservation(data):
    # Check consumption of reactants
    total_w_reactant1_start = np.sum(data[reactant_species[0] + "_w_start"])
    total_w_reactant1_end   = np.sum(data[reactant_species[0] + "_w_end"])
    total_w_reactant2_start = np.sum(data[reactant_species[1] + "_w_start"])
    total_w_reactant2_end   = np.sum(data[reactant_species[1] + "_w_end"])
    consumed_reactant1 = total_w_reactant1_start - total_w_reactant1_end
    consumed_reactant2 = total_w_reactant2_start - total_w_reactant2_end
    assert(consumed_reactant1 >= 0.)
    assert(consumed_reactant2 >= 0.)
    ## Check that number of consumed reactants are equal
    assert_scale = max(total_w_reactant1_start, total_w_reactant2_start)
    assert(is_close(consumed_reactant1, consumed_reactant2, rtol = 0., atol = default_tol*assert_scale))

    # That the number of products corresponds consumed particles
    for species_name in product_species:
        created_product = np.sum(data[species_name + "_w_end"])
        assert(created_product >= 0.)
        assert(is_close(total_w_reactant1_start, total_w_reactant1_end + created_product))
        assert(is_close(total_w_reactant2_start, total_w_reactant2_end + created_product))

def compute_energy_array(data, species_name, suffix, m):
    ## Relativistic computation of kinetic energy for a given species
    psq_array = data[species_name+'_px_'+suffix]**2 + data[species_name+'_py_'+suffix]**2 + \
                data[species_name+'_pz_'+suffix]**2
    rest_energy = m*scc.c**2
    return np.sqrt(psq_array*scc.c**2 + rest_energy**2) - rest_energy

def check_energy_conservation(data):
    total_energy_start = 0
    for species_name in reactant_species:
        total_energy_start +=  np.sum( data[species_name + "_w_start"] * \
            compute_energy_array(data, species_name, "start", mass[species_name]) )
    total_energy_end = 0
    for species_name in product_species + reactant_species:
        total_energy_end +=  np.sum( data[species_name + "_w_end"] * \
            compute_energy_array(data, species_name, "end", mass[species_name]) )
    n_fusion_reaction = np.sum(data[product_species[0] + "_w_end"])
    assert(is_close(total_energy_end,
                    total_energy_start + n_fusion_reaction*E_fusion,
                    rtol = 1.e-8))

def check_momentum_conservation(data):
    total_px_start = 0
    total_py_start = 0
    total_pz_start = 0
    for species_name in reactant_species:
        total_px_start += np.sum(
            data[species_name+'_px_start'] * data[species_name+'_w_start'])
        total_py_start += np.sum(
            data[species_name+'_py_start'] * data[species_name+'_w_start'])
        total_pz_start += np.sum(
            data[species_name+'_pz_start'] * data[species_name+'_w_start'])
    total_px_end = 0
    total_py_end = 0
    total_pz_end = 0
    for species_name in reactant_species + product_species:
        total_px_end += np.sum(
            data[species_name+'_px_end'] * data[species_name+'_w_end'])
        total_py_end += np.sum(
            data[species_name+'_py_end'] * data[species_name+'_w_end'])
        total_pz_end += np.sum(
            data[species_name+'_pz_end'] * data[species_name+'_w_end'])

    ## Absolute tolerance is needed because sometimes the initial momentum is exactly 0
    assert(is_close(total_px_start, total_px_end, atol=1.e-15))
    assert(is_close(total_py_start, total_py_end, atol=1.e-15))
    assert(is_close(total_pz_start, total_pz_end, atol=1.e-15))

def check_id(data):
    ## Check that all created particles have unique id + cpu identifier (two particles with
    ## different cpu can have the same id)
    for species_name in product_species:
        complex_id = data[species_name + "_id_end"] + 1j*data[species_name + "_cpu_end"]
        assert(complex_id.shape == np.unique(complex_id).shape)

def generic_check(data):
    check_particle_number_conservation(data)
    check_energy_conservation(data)
    check_momentum_conservation(data)
    check_id(data)

def check_isotropy(data, relative_tolerance):
    ## Checks that the alpha particles are emitted isotropically
    average_px_sq = np.average(data["alpha_px_end"]*data["alpha_px_end"])
    average_py_sq = np.average(data["alpha_py_end"]*data["alpha_py_end"])
    average_pz_sq = np.average(data["alpha_pz_end"]*data["alpha_pz_end"])
    assert(is_close(average_px_sq, average_py_sq, rtol = relative_tolerance))
    assert(is_close(average_px_sq, average_pz_sq, rtol = relative_tolerance))

def cross_section( E_keV ):
    ## Returns cross section in b, using the analytical fits given
    ## in H.-S. Bosch and G.M. Hale 1992 Nucl. Fusion 32 611
    joule_to_keV = 1.e-3/scc.e
    B_G = scc.pi * scc.alpha * np.sqrt( 2.*m_reduced * scc.c**2 * joule_to_keV );
    A1 = 6.927e4;
    A2 = 7.454e8;
    A3 = 2.050e6;
    A4 = 5.2002e4;
    B1 = 6.38e1;
    B2 = -9.95e-1;
    B3 = 6.981e-5;
    B4 = 1.728e-4;
    astrophysical_factor = (A1 + E_keV*(A2 + E_keV*(A3 + E_keV*A4))) / (1 + E_keV*(B1 + E_keV*(B2 + E_keV*(B3 + E_keV*B4))));
    millibarn_to_barn = 1.e-3;
    return millibarn_to_barn * astrophysical_factor/E_keV * np.exp(-B_G/np.sqrt(E_keV))

def E_com_to_p_sq_com(m1, m2, E):
    ## E is the total (kinetic+mass) energy of a two particle (with mass m1 and m2) system in
    ## its center of mass frame, in J.
    ## Returns the square norm of the momentum of each particle in that frame.
    return E**2/(4.*scc.c**2) - (m1**2 + m2**2)*scc.c**2/2. + \
           scc.c**6/(4.*E**2)*((m1**2 - m2**2)**2)

def compute_relative_v_com(E):
    ## E is the kinetic energy of reactants in the center of mass frame, in keV
    ## Returns the relative velocity between reactants in this frame, in m/s
    m0 = mass[reactant_species[0]]
    m1 = mass[reactant_species[1]]
    E_J  = E*keV_to_Joule + (m0 + m1)*scc.c**2
    p_sq = E_com_to_p_sq_com(m0, m1, E_J)
    p = np.sqrt(p_sq)
    gamma0 = np.sqrt(1. + p_sq / (m0*scc.c)**2)
    gamma1 = np.sqrt(1. + p_sq / (m1*scc.c)**2)
    v0 = p/(gamma0*m0)
    v1 = p/(gamma1*m1)
    return v0+v1

def expected_weight_com(E_com, reactant0_density, reactant1_density, dV, dt):
    ## Computes expected number of product particles as a function of energy E_com in the
    ## center of mass frame. E_com is in keV.
    assert(np.all(E_com>=0))
    ## Case E_com == 0 is handled manually to avoid division by zero
    conditions = [E_com == 0, E_com > 0]
    ## Necessary to avoid division by 0 warning when pb_cross_section is evaluated
    E_com_never_zero = np.clip(E_com, 1.e-15, None)
    choices = [0., cross_section(E_com_never_zero)*compute_relative_v_com(E_com_never_zero)]
    sigma_times_vrel = np.select(conditions, choices)
    return reactant0_density*reactant1_density*sigma_times_vrel*barn_to_square_meter*dV*dt

def check_macroparticle_number(data, fusion_probability_target_value, num_pair_per_cell):
    ## Checks that the number of macroparticles is as expected for the first and second tests

    ## The first slice 0 < z < 1 does not contribute to alpha creation
    numcells = dV_total - dV_slice
    ## In these tests, the fusion_multiplier is so high that the fusion probability per pair is
    ## equal to the parameter fusion_probability_target_value
    fusion_probability_per_pair = fusion_probability_target_value
    expected_fusion_number = numcells*num_pair_per_cell*fusion_probability_per_pair
    expected_macroparticle_number = 2*expected_fusion_number
    std_macroparticle_number = 2*np.sqrt(expected_fusion_number)
    actual_macroparticle_number = data[product_species[0] + "_w_end"].shape[0]
    # 5 sigma test that has an intrinsic probability to fail of 1 over ~2 millions
    assert(is_close(actual_macroparticle_number, expected_macroparticle_number, rtol = 0.,
                    atol = 5.*std_macroparticle_number))

    ## used in subsequent function
    return expected_fusion_number

def p_sq_reactant1_frame_to_E_COM_frame(p_reactant0_sq):
    # Takes the reactant0 square norm of the momentum in the reactant1 rest frame and returns the total
    # kinetic energy in the center of mass frame. Everything is in SI units.
    m0 = mass[reactant_species[0]]
    m1 = mass[reactant_species[1]]

    # Total (kinetic + mass) energy in lab frame
    E_lab = np.sqrt(p_reactant0_sq*scc.c**2 + (m0*scc.c**2)**2) + m1*scc.c**2
    # Use invariant E**2 - p**2c**2 of 4-momentum norm to compute energy in center of mass frame
    E_com = np.sqrt(E_lab**2 - p_reactant0_sq*scc.c**2)
    # Corresponding kinetic energy
    E_com_kin = E_com - (m1+m0)*scc.c**2
    return E_com_kin*(p_reactant0_sq>0.)

def p_sq_to_kinetic_energy(p_sq, m):
    ## Returns the kinetic energy of a particle as a function of its squared momentum.
    ## Everything is in SI units.
    return np.sqrt(p_sq*scc.c**2 + (m*scc.c**2)**2) - (m*scc.c**2)

def compute_E_com1(data):
    ## Computes kinetic energy (in Joule) in the center of frame for the first test

    ## Square norm of the momentum of proton/boron as a function of cell number in z direction
    p_sq = 2.*m_reduced*(Energy_step*np.arange(size_z)**2)
    Ekin = 0
    for species_name in reactant_species:
        Ekin += p_sq_to_kinetic_energy( p_sq, mass[species_name] )
    return Ekin

def compute_E_com2(data):
    ## Computes kinetic energy (in Joule) in the center of frame for the second test

    ## Square norm of the momentum of the proton as a function of cell number in z direction
    p_reactant0_sq = 2.*mass[reactant_species[0]]*(Energy_step*np.arange(size_z)**2)
    return p_sq_reactant1_frame_to_E_COM_frame(p_reactant0_sq)

def check_fusion_yield(data, expected_fusion_number, E_com, reactant0_density, reactant1_density):
    ## Checks that the fusion yield is as expected for the first and second tests.
    product_weight_theory = expected_weight_com(E_com/keV_to_Joule,
        reactant0_density, reactant1_density, dV_slice, dt)
    for species_name in product_species:
        product_weight_simulation = np.histogram(data[species_name+"_z_end"],
                bins=size_z, range=(0, size_z), weights = data[species_name+"_w_end"])[0]

        ## -1 is here because the first slice 0 < z < 1 does not contribute to fusion
        expected_fusion_number_per_slice = expected_fusion_number/(size_z-1)
        relative_std_weight = 1./np.sqrt(expected_fusion_number_per_slice)

        # 5 sigma test that has an intrinsic probability to fail of 1 over ~2 millions
        assert(np.all(is_close(product_weight_theory, product_weight_simulation,
                               rtol = 5.*relative_std_weight)))

def check_initial_energy1(data, E_com):
    ## In WarpX, the initial momentum of the alphas is computed assuming that the fusion process
    ## takes place in two steps:
    ## (1): proton + boron 11 -> alpha + beryllium 8
    ## (2): beryllium 8 -> alpha + alpha
    ## The alpha generated in the first step (labeled alpha1) generally has a different initial
    ## energy distribution than the alphas generated in the second step (labeled alpha2 and
    ## alpha3).
    ## In the first test, we are in the center of mass frame. Therefore, the momentum of alpha1 is
    ## entirely determined by the energy in the center of mass frame, so we check in this function
    ## that the energy of the alpha1 macroparticles is as expected. On the other hand, the energy
    ## of alpha2 and alpha3 follows a continuous distribution within a given range. In this test,
    ## we check that this range is as expected by comparing the maximum and minimum energy of the
    ## obtained macroparticles to the theoretical maximum and minimum.
    ## Note that in the simulations, 6 macroparticles are generated during for each fusion event.
    ## The first and second macroparticles are alpha1 particles. The third and fourth are alpha2.
    ## The fifth and sixth are alpha3.

    energy_alpha_simulation = compute_energy_array(data, "alpha", "end", m_a)
    z_alpha = data["alpha_z_end"]

    # Loop over all slices (i.e. cells in the z direction)
    for slice_number in range(1, size_z):
        ## Kinetic energy in the lab frame before fusion
        E_kinetic_com_before = E_com[slice_number]
        ## Total (kinetic + mass) energy in the lab frame after
        ## proton + boron 11 -> alpha + beryllium 8
        E_total_com_after = E_kinetic_com_before + E_fusion + (m_a + m_be)*scc.c**2
        ## Corresponding momentum norm squared of alpha1/beryllium
        p_sq_after = E_com_to_p_sq_com(m_a, m_be, E_total_com_after)
        ## Corresponding kinetic energy for alpha1
        energy_alpha1_theory = p_sq_to_kinetic_energy(p_sq_after, m_a)
        ## Corresponding kinetic energy for beryllium
        energy_beryllium_theory = p_sq_to_kinetic_energy(p_sq_after, m_be)
        ## Corresponding kinetic energy for alpha2 + alpha3 after beryllium decay
        energy_alpha2_plus_3_theory = energy_beryllium_theory + E_decay
        ## Compute the theoretical maximum and minimum energy of alpha2 and alpha3. This
        ## calculation is done nonrelativistically, by noting that the maximum (minimum) energy
        ## corresponds to an alpha emitted exactly in the (opposite) direction of the beryllium
        ## in the center of mass frame. This calculation involves solving a polynomial equation of
        ## order 2 in p_alpha23.
        max_p_alpha23 = 0.5*(np.sqrt(p_sq_after) + \
                        np.sqrt(4*m_a*energy_alpha2_plus_3_theory - p_sq_after))
        min_p_alpha23 = 0.5*(np.sqrt(p_sq_after) - \
                        np.sqrt(4*m_a*energy_alpha2_plus_3_theory - p_sq_after))
        max_energy_alpha23 = max_p_alpha23**2/(2.*m_a)
        min_energy_alpha23 = min_p_alpha23**2/(2.*m_a)

        ## Get the energy of all alphas in the slice
        energy_alpha_slice = energy_alpha_simulation[(z_alpha >= slice_number)* \
                                                     (z_alpha < (slice_number + 1))]
        ## Energy of alphas1 (here, first macroparticle of each fusion event) in the slice
        energy_alpha1_simulation = energy_alpha_slice[::6]
        ## Energy of alphas2 (here, third macroparticle of each fusion event) in the slice
        energy_alpha2_simulation = energy_alpha_slice[2::6]
        ## Energy of alphas3 (here, fifth macroparticle of each fusion event) in the slice
        energy_alpha3_simulation = energy_alpha_slice[4::6]

        assert(np.all(is_close(energy_alpha1_simulation, energy_alpha1_theory, rtol=5.e-8)))
        assert(is_close(np.amax(energy_alpha2_simulation), max_energy_alpha23, rtol=1.e-2))
        assert(is_close(np.amin(energy_alpha2_simulation), min_energy_alpha23, rtol=1.e-2))
        assert(is_close(np.amax(energy_alpha3_simulation), max_energy_alpha23, rtol=1.e-2))
        assert(is_close(np.amin(energy_alpha3_simulation), min_energy_alpha23, rtol=1.e-2))

def check_initial_energy2(data):
    ## In WarpX, the initial momentum of the alphas is computed assuming that the fusion process
    ## takes place in two steps:
    ## (1): proton + boron 11 -> alpha + beryllium 8
    ## (2): beryllium 8 -> alpha + alpha
    ## The alpha generated in the first step (labeled alpha1) generally has a different initial
    ## energy distribution than the alphas generated in the second step (labeled alpha2 and
    ## alpha3).
    ## In the second test, we are in the boron rest frame. In this case, the momentum of each alpha
    ## follows a continuous distribution within a given range. In this function, we verify that
    ## this range is as expected by comparing the maximum and minimum energy of the obtained
    ## macroparticles to the theoretical maximum and minimum. Be aware that the range for alpha1
    ## is not the same as the range for alpha2 and alpha3 (typically alpha1 particles will carry
    ## more energy).
    ## Note that in the simulations, 6 macroparticles are generated during for each fusion event.
    ## The first and second macroparticles are alpha1 particles. The third and fourth are alpha2.
    ## The fifth and sixth are alpha3.

    energy_alpha_simulation = compute_energy_array(data, "alpha", "end", m_a)
    z_alpha = data["alpha_z_end"]

    # Loop over all slices (i.e. cells in the z direction)
    for slice_number in range(1, size_z):
        ## For simplicity, all the calculations in this functino are done nonrelativistically
        ## Proton kinetic energy in the lab frame before fusion
        E_proton_nonrelativistic = Energy_step*slice_number**2
        ## Corresponding square norm of proton momentum
        p_proton_sq = 2.*scc.m_p*E_proton_nonrelativistic
        ## Kinetic energy in the lab frame after
        ## proton + boron 11 -> alpha + beryllium 8
        E_after_fusion = E_proton_nonrelativistic + E_fusion

        ## Compute the theoretical maximum and minimum energy of alpha1 in the lab frame. This
        ## calculation is done by noting that the maximum (minimum) energy corresponds to an alpha
        ## emitted exactly in the (opposite) direction of the proton in the lab frame. This
        ## calculation involves solving a polynomial equation of order 2 in p_alpha1.
        max_p_alpha1 = (m_a/m_be*np.sqrt(p_proton_sq) + \
                       np.sqrt(-m_a/m_be*p_proton_sq + 2.*E_after_fusion*m_a*(m_a/m_be + 1.))) / \
                       (m_a/m_be + 1.)
        min_p_alpha1 = (m_a/m_be*np.sqrt(p_proton_sq) - \
                       np.sqrt(-m_a/m_be*p_proton_sq + 2.*E_after_fusion*m_a*(m_a/m_be + 1.))) / \
                       (m_a/m_be + 1.)
        max_energy_alpha1 = max_p_alpha1**2/(2*m_a)
        min_energy_alpha1 = min_p_alpha1**2/(2*m_a)

        ## Corresponding max/min kinetic energy of Beryllium in the lab frame
        max_E_beryllium = E_after_fusion - min_energy_alpha1
        min_E_beryllium = E_after_fusion - max_energy_alpha1
        ## Corresponding max/min momentum square of Beryllium in the lab frame
        max_p_sq_beryllium = 2.*m_be*max_E_beryllium
        min_p_sq_beryllium = 2.*m_be*min_E_beryllium
        ## Corresponding max/min kinetic energy in the lab frame for alpha2 + alpha3 after
        ## Beryllium decay
        max_energy_alpha2_plus_3 = max_E_beryllium + E_decay
        min_energy_alpha2_plus_3 = min_E_beryllium + E_decay

        ## Compute the theoretical maximum and minimum energy of alpha2 and alpha3 in the lab
        ## frame. This calculation is done by noting that the maximum (minimum) energy corresponds
        ## to an alpha emitted exactly in the (opposite) direction of a beryllium with energy
        ## max_E_beryllium (min_E_beryllium). This calculation involves solving a polynomial
        ## equation of order 2 in p_alpha23.
        max_p_alpha23 = 0.5*(np.sqrt(max_p_sq_beryllium) + \
                             np.sqrt(4*m_a*max_energy_alpha2_plus_3 - max_p_sq_beryllium))
        min_p_alpha23 = 0.5*(np.sqrt(min_p_sq_beryllium) - \
                             np.sqrt(4*m_a*min_energy_alpha2_plus_3 - min_p_sq_beryllium))
        max_energy_alpha23 = max_p_alpha23**2/(2*m_a)
        min_energy_alpha23 = min_p_alpha23**2/(2*m_a)

        ## Get the energy of all alphas in the slice
        energy_alpha_slice = energy_alpha_simulation[(z_alpha >= slice_number)* \
                                                     (z_alpha < (slice_number + 1))]
        ## Energy of alphas1 (here, first macroparticle of each fusion event) in the slice
        energy_alpha1_simulation = energy_alpha_slice[::6]
        ## Energy of alphas2 (here, third macroparticle of each fusion event) in the slice
        energy_alpha2_simulation = energy_alpha_slice[2::6]
        ## Energy of alphas3 (here, fifth macroparticle of each fusion event) in the slice
        energy_alpha3_simulation = energy_alpha_slice[4::6]

        assert(is_close(np.amax(energy_alpha1_simulation), max_energy_alpha1, rtol=1.e-2))
        assert(is_close(np.amin(energy_alpha1_simulation), min_energy_alpha1, rtol=1.e-2))
        ## Tolerance is quite high below because we don't have a lot of alphas to produce good
        ## statistics and an event like alpha1 emitted exactly in direction of proton & alpha2
        ## emitted exactly in direction opposite to Beryllium is somewhat rare.
        assert(is_close(np.amax(energy_alpha2_simulation), max_energy_alpha23, rtol=2.5e-1))
        assert(is_close(np.amin(energy_alpha2_simulation), min_energy_alpha23, rtol=2.5e-1))
        assert(is_close(np.amax(energy_alpha3_simulation), max_energy_alpha23, rtol=2.5e-1))
        assert(is_close(np.amin(energy_alpha3_simulation), min_energy_alpha23, rtol=2.5e-1))

def check_xy_isotropy(data):
    ## Checks that the alpha particles are emitted isotropically in x and y
    average_px_sq = np.average(data["alpha_px_end"]*data["alpha_px_end"])
    average_py_sq = np.average(data["alpha_py_end"]*data["alpha_py_end"])
    average_pz_sq = np.average(data["alpha_pz_end"]*data["alpha_pz_end"])
    assert(is_close(average_px_sq, average_py_sq, rtol = 5.e-2))
    assert(average_pz_sq > average_px_sq)
    assert(average_pz_sq > average_py_sq)

def specific_check1(data):
    #check_isotropy(data, relative_tolerance = 3.e-2)
    expected_fusion_number = check_macroparticle_number(data,
                                                        fusion_probability_target_value = 0.002,
                                                        num_pair_per_cell = 10000)
    E_com = compute_E_com1(data)
    check_fusion_yield(data, expected_fusion_number, E_com, reactant0_density = 1.,
                                                           reactant1_density = 1.)
    #check_initial_energy1(data, E_com)

def specific_check2(data):
    #check_xy_isotropy(data)
    ## Only 900 particles pairs per cell here because we ignore the 10% of reactants that are at rest
    expected_fusion_number = check_macroparticle_number(data,
                                                        fusion_probability_target_value = 0.02,
                                                        num_pair_per_cell = 900)
    E_com = compute_E_com2(data)
    check_fusion_yield(data, expected_fusion_number, E_com, reactant0_density = 1.e20,
                                                           reactant1_density = 1.e26)
    #check_initial_energy2(data)

def check_charge_conservation(rho_start, rho_end):
    assert(np.all(is_close(rho_start, rho_end, rtol=2.e-11)))

def main():
    filename_end = sys.argv[1]
    filename_start = filename_end[:-4] + '0000'
    ds_end = yt.load(filename_end)
    ds_start = yt.load(filename_start)
    ad_end = ds_end.all_data()
    ad_start = ds_start.all_data()
    field_data_end = ds_end.covering_grid(level=0, left_edge=ds_end.domain_left_edge,
                                          dims=ds_end.domain_dimensions)
    field_data_start = ds_start.covering_grid(level=0, left_edge=ds_start.domain_left_edge,
                                              dims=ds_start.domain_dimensions)

    ntests = 2
    for i in range(1, ntests+1):
        data = {}

        for species_name in reactant_species:
            add_species_to_dict(ad_start, data, species_name+str(i), species_name, "start")
            add_species_to_dict(ad_end, data, species_name+str(i), species_name, "end")

        for species_name in product_species:
            add_species_to_dict(ad_end, data, species_name+str(i), species_name, "end")

        # General checks that are performed for all tests
        generic_check(data)

        # Checks that are specific to test number i
        eval("specific_check"+str(i)+"(data)")

    rho_start = field_data_start["rho"].to_ndarray()
    rho_end = field_data_end["rho"].to_ndarray()
    check_charge_conservation(rho_start, rho_end)

    test_name = os.path.split(os.getcwd())[1]
    checksumAPI.evaluate_checksum(test_name, filename_end)

if __name__ == "__main__":
    main()
