/* Copyright 2019 Andrew Myers, Aurore Blelly, Axel Huebl
 * David Grote, Maxence Thevenet, Remi Lehe
 * Revathi Jambunathan, Weiqun Zhang
 *
 * This file is part of WarpX.
 *
 * License: BSD-3-Clause-LBNL
 */
#include "WarpX.H"
#include "Utils/WarpXConst.H"
#include "BoundaryConditions/WarpX_PML_kernels.H"
#include "BoundaryConditions/PML_current.H"
#include "WarpX_FDTD.H"
#include "WarpXPushFieldsEM_K.H"

#ifdef BL_USE_SENSEI_INSITU
#   include <AMReX_AmrMeshInSituBridge.H>
#endif

#include <AMReX.H>
#include <AMReX_Math.H>
#include <limits>


using namespace amrex;

#ifdef WARPX_USE_PSATD
namespace {

    void
    ForwardTransformVect (
        const int lev,
#ifdef WARPX_DIM_RZ
        SpectralSolverRZ& solver,
#else
        SpectralSolver& solver,
#endif
        std::array<std::unique_ptr<amrex::MultiFab>,3>& vector_field,
        const int compx, const int compy, const int compz)
    {
#ifdef WARPX_DIM_RZ
        solver.ForwardTransform(lev, *vector_field[0], compx, *vector_field[1], compy);
#else
        solver.ForwardTransform(lev, *vector_field[0], compx);
        solver.ForwardTransform(lev, *vector_field[1], compy);
#endif
        solver.ForwardTransform(lev, *vector_field[2], compz);
    }

    void
    BackwardTransformVect (
        const int lev,
#ifdef WARPX_DIM_RZ
        SpectralSolverRZ& solver,
#else
        SpectralSolver& solver,
#endif
        std::array<std::unique_ptr<amrex::MultiFab>,3>& vector_field,
        const int compx, const int compy, const int compz)
    {
#ifdef WARPX_DIM_RZ
        solver.BackwardTransform(lev, *vector_field[0], compx, *vector_field[1], compy);
#else
        solver.BackwardTransform(lev, *vector_field[0], compx);
        solver.BackwardTransform(lev, *vector_field[1], compy);
#endif
        solver.BackwardTransform(lev, *vector_field[2], compz);
    }
}

using Idx = SpectralAvgFieldIndex;
using LinJIdx = SpectralFieldIndexLinearInJ;

void
WarpX::PSATDForwardTransformEB ()
{
    for (int lev = 0; lev <= finest_level; ++lev)
    {
        ForwardTransformVect(lev, *spectral_solver_fp[lev], Efield_fp[lev], Idx::Ex, Idx::Ey, Idx::Ez);
        ForwardTransformVect(lev, *spectral_solver_fp[lev], Bfield_fp[lev], Idx::Bx, Idx::By, Idx::Bz);

        if (spectral_solver_cp[lev])
        {
            ForwardTransformVect(lev, *spectral_solver_cp[lev], Efield_cp[lev], Idx::Ex, Idx::Ey, Idx::Ez);
            ForwardTransformVect(lev, *spectral_solver_cp[lev], Bfield_cp[lev], Idx::Bx, Idx::By, Idx::Bz);
        }
    }
}

void
WarpX::PSATDBackwardTransformEB ()
{
    for (int lev = 0; lev <= finest_level; ++lev)
    {
        BackwardTransformVect(lev, *spectral_solver_fp[lev], Efield_fp[lev], Idx::Ex, Idx::Ey, Idx::Ez);
        BackwardTransformVect(lev, *spectral_solver_fp[lev], Bfield_fp[lev], Idx::Bx, Idx::By, Idx::Bz);

        if (spectral_solver_cp[lev])
        {
            BackwardTransformVect(lev, *spectral_solver_cp[lev], Efield_cp[lev], Idx::Ex, Idx::Ey, Idx::Ez);
            BackwardTransformVect(lev, *spectral_solver_cp[lev], Bfield_cp[lev], Idx::Bx, Idx::By, Idx::Bz);
        }
    }

    // Damp the fields in the guard cells along z
    constexpr int zdir = AMREX_SPACEDIM - 1;
    if (WarpX::field_boundary_lo[zdir] == FieldBoundaryType::Damped &&
        WarpX::field_boundary_hi[zdir] == FieldBoundaryType::Damped)
    {
        for (int lev = 0; lev <= finest_level; ++lev)
        {
            DampFieldsInGuards(Efield_fp[lev], Bfield_fp[lev]);
        }
    }
}

void
WarpX::PSATDBackwardTransformEBavg ()
{
    for (int lev = 0; lev <= finest_level; ++lev)
    {
        BackwardTransformVect(lev, *spectral_solver_fp[lev], Efield_avg_fp[lev], Idx::Ex_avg, Idx::Ey_avg, Idx::Ez_avg);
        BackwardTransformVect(lev, *spectral_solver_fp[lev], Bfield_avg_fp[lev], Idx::Bx_avg, Idx::By_avg, Idx::Bz_avg);

        if (spectral_solver_cp[lev])
        {
            BackwardTransformVect(lev, *spectral_solver_cp[lev], Efield_avg_cp[lev], Idx::Ex_avg, Idx::Ey_avg, Idx::Ez_avg);
            BackwardTransformVect(lev, *spectral_solver_cp[lev], Bfield_avg_cp[lev], Idx::Bx_avg, Idx::By_avg, Idx::Bz_avg);
        }
    }
}

void
WarpX::PSATDForwardTransformF ()
{
    for (int lev = 0; lev <= finest_level; ++lev)
    {
        if (F_fp[lev]) spectral_solver_fp[lev]->ForwardTransform(lev, *F_fp[lev], LinJIdx::F);

        if (spectral_solver_cp[lev])
        {
            if (F_cp[lev]) spectral_solver_cp[lev]->ForwardTransform(lev, *F_cp[lev], LinJIdx::F);
        }
    }
}

void
WarpX::PSATDBackwardTransformF ()
{
    for (int lev = 0; lev <= finest_level; ++lev)
    {
        if (F_fp[lev]) spectral_solver_fp[lev]->BackwardTransform(lev, *F_fp[lev], LinJIdx::F);

        if (spectral_solver_cp[lev])
        {
            if (F_cp[lev]) spectral_solver_cp[lev]->BackwardTransform(lev, *F_cp[lev], LinJIdx::F);
        }
    }
}

void
WarpX::PSATDForwardTransformG ()
{
    for (int lev = 0; lev <= finest_level; ++lev)
    {
        if (G_fp[lev]) spectral_solver_fp[lev]->ForwardTransform(lev, *G_fp[lev], LinJIdx::G);

        if (spectral_solver_cp[lev])
        {
            if (G_cp[lev]) spectral_solver_cp[lev]->ForwardTransform(lev, *G_cp[lev], LinJIdx::G);
        }
    }
}

void
WarpX::PSATDBackwardTransformG ()
{
    for (int lev = 0; lev <= finest_level; ++lev)
    {
        if (G_fp[lev]) spectral_solver_fp[lev]->BackwardTransform(lev, *G_fp[lev], LinJIdx::G);

        if (spectral_solver_cp[lev])
        {
            if (G_cp[lev]) spectral_solver_cp[lev]->BackwardTransform(lev, *G_cp[lev], LinJIdx::G);
        }
    }
}

void
WarpX::PSATDForwardTransformJ ()
{
    int idx_jx = Idx::Jx;
    int idx_jy = Idx::Jy;
    int idx_jz = Idx::Jz;

    if (WarpX::J_linear_in_time)
    {
        idx_jx = LinJIdx::Jx_new;
        idx_jy = LinJIdx::Jy_new;
        idx_jz = LinJIdx::Jz_new;
    }

    for (int lev = 0; lev <= finest_level; ++lev)
    {
        ForwardTransformVect(lev, *spectral_solver_fp[lev], current_fp[lev], idx_jx, idx_jy, idx_jz);

        if (spectral_solver_cp[lev])
        {
            ForwardTransformVect(lev, *spectral_solver_cp[lev], current_cp[lev], idx_jx, idx_jy, idx_jz);
        }
    }

#ifdef WARPX_DIM_RZ
    // Apply filter in k space if needed
    if (WarpX::use_kspace_filter)
    {
        for (int lev = 0; lev <= finest_level; ++lev)
        {
            spectral_solver_fp[lev]->ApplyFilter(Idx::Jx, Idx::Jy, Idx::Jz);

            if (spectral_solver_cp[lev])
            {
                spectral_solver_cp[lev]->ApplyFilter(Idx::Jx, Idx::Jy, Idx::Jz);
            }
        }
    }
#endif
}

void
WarpX::PSATDForwardTransformRho (const int icomp)
{
    // Select index in k space
    int const dst_comp = (icomp==0 ? Idx::rho_old : Idx::rho_new);

    for (int lev = 0; lev <= finest_level; ++lev)
    {
        if (rho_fp[lev]) spectral_solver_fp[lev]->ForwardTransform(lev, *rho_fp[lev], dst_comp, icomp);

        if (spectral_solver_cp[lev])
        {
            if (rho_cp[lev]) spectral_solver_cp[lev]->ForwardTransform(lev, *rho_cp[lev], dst_comp, icomp);
        }
    }

#ifdef WARPX_DIM_RZ
    // Apply filter in k space if needed
    if (WarpX::use_kspace_filter)
    {
        for (int lev = 0; lev <= finest_level; ++lev)
        {
            spectral_solver_fp[lev]->ApplyFilter(dst_comp);

            if (spectral_solver_cp[lev])
            {
                spectral_solver_cp[lev]->ApplyFilter(dst_comp);
            }
        }
    }
#endif
}

void
WarpX::PSATDPushSpectralFields ()
{
    for (int lev = 0; lev <= finest_level; ++lev)
    {
        spectral_solver_fp[lev]->pushSpectralFields();

        if (spectral_solver_cp[lev])
        {
            spectral_solver_cp[lev]->pushSpectralFields();
        }
    }
}

#ifndef WARPX_DIM_RZ
void
WarpX::PSATDMoveRhoNewToRhoOld ()
{
    for (int lev = 0; lev <= finest_level; ++lev)
    {
        spectral_solver_fp[lev]->CopySpectralDataComp(Idx::rho_new, Idx::rho_old);

        if (spectral_solver_cp[lev])
        {
            spectral_solver_cp[lev]->CopySpectralDataComp(Idx::rho_new, Idx::rho_old);
        }
    }
}

void
WarpX::PSATDMoveJNewToJOld ()
{
    for (int lev = 0; lev <= finest_level; ++lev)
    {
        spectral_solver_fp[lev]->CopySpectralDataComp(LinJIdx::Jx_new, LinJIdx::Jx_old);
        spectral_solver_fp[lev]->CopySpectralDataComp(LinJIdx::Jy_new, LinJIdx::Jy_old);
        spectral_solver_fp[lev]->CopySpectralDataComp(LinJIdx::Jz_new, LinJIdx::Jz_old);

        if (spectral_solver_cp[lev])
        {
            spectral_solver_cp[lev]->CopySpectralDataComp(LinJIdx::Jx_new, LinJIdx::Jx_old);
            spectral_solver_cp[lev]->CopySpectralDataComp(LinJIdx::Jy_new, LinJIdx::Jy_old);
            spectral_solver_cp[lev]->CopySpectralDataComp(LinJIdx::Jz_new, LinJIdx::Jz_old);
        }
    }
}

void
WarpX::PSATDEraseAverageFields ()
{
    for (int lev = 0; lev <= finest_level; ++lev)
    {
        spectral_solver_fp[lev]->ZeroOutDataComp(Idx::Ex_avg);
        spectral_solver_fp[lev]->ZeroOutDataComp(Idx::Ey_avg);
        spectral_solver_fp[lev]->ZeroOutDataComp(Idx::Ez_avg);
        spectral_solver_fp[lev]->ZeroOutDataComp(Idx::Bx_avg);
        spectral_solver_fp[lev]->ZeroOutDataComp(Idx::By_avg);
        spectral_solver_fp[lev]->ZeroOutDataComp(Idx::Bz_avg);

        if (spectral_solver_cp[lev])
        {
            spectral_solver_cp[lev]->ZeroOutDataComp(Idx::Ex_avg);
            spectral_solver_cp[lev]->ZeroOutDataComp(Idx::Ey_avg);
            spectral_solver_cp[lev]->ZeroOutDataComp(Idx::Ez_avg);
            spectral_solver_cp[lev]->ZeroOutDataComp(Idx::Bx_avg);
            spectral_solver_cp[lev]->ZeroOutDataComp(Idx::By_avg);
            spectral_solver_cp[lev]->ZeroOutDataComp(Idx::Bz_avg);
        }
    }
}

void
WarpX::PSATDScaleAverageFields (const amrex::Real scale_factor)
{
    for (int lev = 0; lev <= finest_level; ++lev)
    {
        spectral_solver_fp[lev]->ScaleDataComp(Idx::Ex_avg, scale_factor);
        spectral_solver_fp[lev]->ScaleDataComp(Idx::Ey_avg, scale_factor);
        spectral_solver_fp[lev]->ScaleDataComp(Idx::Ez_avg, scale_factor);
        spectral_solver_fp[lev]->ScaleDataComp(Idx::Bx_avg, scale_factor);
        spectral_solver_fp[lev]->ScaleDataComp(Idx::By_avg, scale_factor);
        spectral_solver_fp[lev]->ScaleDataComp(Idx::Bz_avg, scale_factor);

        if (spectral_solver_cp[lev])
        {
            spectral_solver_cp[lev]->ScaleDataComp(Idx::Ex_avg, scale_factor);
            spectral_solver_cp[lev]->ScaleDataComp(Idx::Ey_avg, scale_factor);
            spectral_solver_cp[lev]->ScaleDataComp(Idx::Ez_avg, scale_factor);
            spectral_solver_cp[lev]->ScaleDataComp(Idx::Bx_avg, scale_factor);
            spectral_solver_cp[lev]->ScaleDataComp(Idx::By_avg, scale_factor);
            spectral_solver_cp[lev]->ScaleDataComp(Idx::Bz_avg, scale_factor);
        }
    }
}
#endif // not WARPX_DIM_RZ
#endif // WARPX_USE_PSATD

void
WarpX::PushPSATD ()
{
#ifndef WARPX_USE_PSATD
    amrex::Abort("PushFieldsEM: PSATD solver selected but not built");
#else

    PSATDForwardTransformEB();
    PSATDForwardTransformJ();
    PSATDForwardTransformRho(0); // rho old
    PSATDForwardTransformRho(1); // rho new
    PSATDPushSpectralFields();
    PSATDBackwardTransformEB();
    if (WarpX::fft_do_time_averaging) PSATDBackwardTransformEBavg();

    // Evolve the fields in the PML boxes
    for (int lev = 0; lev <= finest_level; ++lev)
    {
        if (do_pml && pml[lev]->ok())
        {
            pml[lev]->PushPSATD(lev);
        }
        ApplyEfieldBoundary(lev, PatchType::fine);
        if (lev > 0) ApplyEfieldBoundary(lev, PatchType::coarse);
        ApplyBfieldBoundary(lev, PatchType::fine);
        if (lev > 0) ApplyBfieldBoundary(lev, PatchType::coarse);
    }
#endif
}

void
WarpX::EvolveB (amrex::Real a_dt)
{
    for (int lev = 0; lev <= finest_level; ++lev) {
        EvolveB(lev, a_dt);
    }
}

void
WarpX::EvolveB (int lev, amrex::Real a_dt)
{
    WARPX_PROFILE("WarpX::EvolveB()");
    EvolveB(lev, PatchType::fine, a_dt);
    if (lev > 0)
    {
        EvolveB(lev, PatchType::coarse, a_dt);
    }
}

void
WarpX::EvolveB (int lev, PatchType patch_type, amrex::Real a_dt)
{

    // Evolve B field in regular cells
    if (patch_type == PatchType::fine) {
        m_fdtd_solver_fp[lev]->EvolveB(Bfield_fp[lev], Efield_fp[lev], G_fp[lev],
                                        m_face_areas[lev], lev, a_dt);
    } else {
        m_fdtd_solver_cp[lev]->EvolveB(Bfield_cp[lev], Efield_cp[lev], G_cp[lev],
                                        m_face_areas[lev], lev, a_dt);
    }

    // Evolve B field in PML cells
    if (do_pml && pml[lev]->ok()) {
        if (patch_type == PatchType::fine) {
            m_fdtd_solver_fp[lev]->EvolveBPML(
                pml[lev]->GetB_fp(), pml[lev]->GetE_fp(), a_dt, WarpX::do_dive_cleaning);
        } else {
            m_fdtd_solver_cp[lev]->EvolveBPML(
                pml[lev]->GetB_cp(), pml[lev]->GetE_cp(), a_dt, WarpX::do_dive_cleaning);
        }
    }

    ApplyBfieldBoundary(lev, patch_type);
}

void
WarpX::ApplySilverMuellerBoundary (amrex::Real a_dt) {
    // Only apply to level 0
    m_fdtd_solver_fp[0]->ApplySilverMuellerBoundary(
        Efield_fp[0], Bfield_fp[0], Geom(0).Domain(), a_dt );
}

void
WarpX::EvolveE (amrex::Real a_dt)
{
    for (int lev = 0; lev <= finest_level; ++lev)
    {
        EvolveE(lev, a_dt);
    }
}

void
WarpX::EvolveE (int lev, amrex::Real a_dt)
{
    WARPX_PROFILE("WarpX::EvolveE()");
    EvolveE(lev, PatchType::fine, a_dt);
    if (lev > 0)
    {
        EvolveE(lev, PatchType::coarse, a_dt);
    }
}

void
WarpX::EvolveE (int lev, PatchType patch_type, amrex::Real a_dt)
{
    // Evolve E field in regular cells
    if (patch_type == PatchType::fine) {
        m_fdtd_solver_fp[lev]->EvolveE(Efield_fp[lev], Bfield_fp[lev],
                                       current_fp[lev], m_edge_lengths[lev],
                                       F_fp[lev], lev, a_dt );
    } else {
        m_fdtd_solver_cp[lev]->EvolveE(Efield_cp[lev], Bfield_cp[lev],
                                       current_cp[lev], m_edge_lengths[lev],
                                       F_cp[lev], lev, a_dt );
    }

    // Evolve E field in PML cells
    if (do_pml && pml[lev]->ok()) {
        if (patch_type == PatchType::fine) {
            m_fdtd_solver_fp[lev]->EvolveEPML(
                pml[lev]->GetE_fp(), pml[lev]->GetB_fp(),
                pml[lev]->Getj_fp(), pml[lev]->GetF_fp(),
                pml[lev]->GetMultiSigmaBox_fp(),
                a_dt, pml_has_particles );
        } else {
            m_fdtd_solver_cp[lev]->EvolveEPML(
                pml[lev]->GetE_cp(), pml[lev]->GetB_cp(),
                pml[lev]->Getj_cp(), pml[lev]->GetF_cp(),
                pml[lev]->GetMultiSigmaBox_cp(),
                a_dt, pml_has_particles );
        }
    }

    ApplyEfieldBoundary(lev, patch_type);

}


void
WarpX::EvolveF (amrex::Real a_dt, DtType a_dt_type)
{
    if (!do_dive_cleaning) return;

    for (int lev = 0; lev <= finest_level; ++lev)
    {
        EvolveF(lev, a_dt, a_dt_type);
    }
}

void
WarpX::EvolveF (int lev, amrex::Real a_dt, DtType a_dt_type)
{
    if (!do_dive_cleaning) return;

    EvolveF(lev, PatchType::fine, a_dt, a_dt_type);
    if (lev > 0) EvolveF(lev, PatchType::coarse, a_dt, a_dt_type);
}

void
WarpX::EvolveF (int lev, PatchType patch_type, amrex::Real a_dt, DtType a_dt_type)
{
    if (!do_dive_cleaning) return;

    WARPX_PROFILE("WarpX::EvolveF()");

    const int rhocomp = (a_dt_type == DtType::FirstHalf) ? 0 : 1;

    // Evolve F field in regular cells
    if (patch_type == PatchType::fine) {
        m_fdtd_solver_fp[lev]->EvolveF( F_fp[lev], Efield_fp[lev],
                                        rho_fp[lev], rhocomp, a_dt );
    } else {
        m_fdtd_solver_cp[lev]->EvolveF( F_cp[lev], Efield_cp[lev],
                                        rho_cp[lev], rhocomp, a_dt );
    }

    // Evolve F field in PML cells
    if (do_pml && pml[lev]->ok()) {
        if (patch_type == PatchType::fine) {
            m_fdtd_solver_fp[lev]->EvolveFPML(
                pml[lev]->GetF_fp(), pml[lev]->GetE_fp(), a_dt );
        } else {
            m_fdtd_solver_cp[lev]->EvolveFPML(
                pml[lev]->GetF_cp(), pml[lev]->GetE_cp(), a_dt );
        }
    }
}

void
WarpX::EvolveG (amrex::Real a_dt, DtType a_dt_type)
{
    if (!do_divb_cleaning) return;

    for (int lev = 0; lev <= finest_level; ++lev)
    {
        EvolveG(lev, a_dt, a_dt_type);
    }
}

void
WarpX::EvolveG (int lev, amrex::Real a_dt, DtType a_dt_type)
{
    if (!do_divb_cleaning) return;

    EvolveG(lev, PatchType::fine, a_dt, a_dt_type);

    if (lev > 0)
    {
        EvolveG(lev, PatchType::coarse, a_dt, a_dt_type);
    }
}

void
WarpX::EvolveG (int lev, PatchType patch_type, amrex::Real a_dt, DtType /*a_dt_type*/)
{
    if (!do_divb_cleaning) return;

    WARPX_PROFILE("WarpX::EvolveG()");

    // Evolve G field in regular cells
    if (patch_type == PatchType::fine)
    {
        m_fdtd_solver_fp[lev]->EvolveG(G_fp[lev], Bfield_fp[lev], a_dt);
    }
    else // coarse patch
    {
        m_fdtd_solver_cp[lev]->EvolveG(G_cp[lev], Bfield_cp[lev], a_dt);
    }

    // TODO Evolution in PML cells will go here
}

void
WarpX::MacroscopicEvolveE (amrex::Real a_dt)
{
    for (int lev = 0; lev <= finest_level; ++lev ) {
        MacroscopicEvolveE(lev, a_dt);
    }
}

void
WarpX::MacroscopicEvolveE (int lev, amrex::Real a_dt) {

    WARPX_PROFILE("WarpX::MacroscopicEvolveE()");
    MacroscopicEvolveE(lev, PatchType::fine, a_dt);
    if (lev > 0) {
        amrex::Abort("Macroscopic EvolveE is not implemented for lev>0, yet.");
    }
}

void
WarpX::MacroscopicEvolveE (int lev, PatchType patch_type, amrex::Real a_dt) {
    if (patch_type == PatchType::fine) {
        m_fdtd_solver_fp[lev]->MacroscopicEvolveE( Efield_fp[lev], Bfield_fp[lev],
                                             current_fp[lev], a_dt,
                                             m_macroscopic_properties);
    }
    else {
        amrex::Abort("Macroscopic EvolveE is not implemented for lev > 0, yet.");
    }
    if (do_pml && pml[lev]->ok()) {
        if (patch_type == PatchType::fine) {
            m_fdtd_solver_fp[lev]->EvolveEPML(
                pml[lev]->GetE_fp(), pml[lev]->GetB_fp(),
                pml[lev]->Getj_fp(), pml[lev]->GetF_fp(),
                pml[lev]->GetMultiSigmaBox_fp(),
                a_dt, pml_has_particles );
        } else {
            m_fdtd_solver_cp[lev]->EvolveEPML(
                pml[lev]->GetE_cp(), pml[lev]->GetB_cp(),
                pml[lev]->Getj_cp(), pml[lev]->GetF_cp(),
                pml[lev]->GetMultiSigmaBox_cp(),
                a_dt, pml_has_particles );
        }
    }

    ApplyEfieldBoundary(lev, patch_type);
}

void
WarpX::DampFieldsInGuards(std::array<std::unique_ptr<amrex::MultiFab>,3>& Efield,
                          std::array<std::unique_ptr<amrex::MultiFab>,3>& Bfield) {

    constexpr int zdir = (AMREX_SPACEDIM - 1);

    for ( amrex::MFIter mfi(*Efield[0], amrex::TilingIfNotGPU()); mfi.isValid(); ++mfi )
    {
        amrex::Array4<amrex::Real> const& Ex_arr = Efield[0]->array(mfi);
        amrex::Array4<amrex::Real> const& Ey_arr = Efield[1]->array(mfi);
        amrex::Array4<amrex::Real> const& Ez_arr = Efield[2]->array(mfi);
        amrex::Array4<amrex::Real> const& Bx_arr = Bfield[0]->array(mfi);
        amrex::Array4<amrex::Real> const& By_arr = Bfield[1]->array(mfi);
        amrex::Array4<amrex::Real> const& Bz_arr = Bfield[2]->array(mfi);

        // Get the tileboxes from Efield and Bfield so that they include the guard cells
        // and take the staggering of each MultiFab into account
        amrex::Box tex = amrex::convert((*Efield[0])[mfi].box(), Efield[0]->ixType().toIntVect());
        amrex::Box tey = amrex::convert((*Efield[1])[mfi].box(), Efield[1]->ixType().toIntVect());
        amrex::Box tez = amrex::convert((*Efield[2])[mfi].box(), Efield[2]->ixType().toIntVect());
        amrex::Box tbx = amrex::convert((*Bfield[0])[mfi].box(), Bfield[0]->ixType().toIntVect());
        amrex::Box tby = amrex::convert((*Bfield[1])[mfi].box(), Bfield[1]->ixType().toIntVect());
        amrex::Box tbz = amrex::convert((*Bfield[2])[mfi].box(), Bfield[2]->ixType().toIntVect());

        // Get smallEnd of tileboxes
        const int tex_smallEnd_z = tex.smallEnd(zdir);
        const int tey_smallEnd_z = tey.smallEnd(zdir);
        const int tez_smallEnd_z = tez.smallEnd(zdir);
        const int tbx_smallEnd_z = tbx.smallEnd(zdir);
        const int tby_smallEnd_z = tby.smallEnd(zdir);
        const int tbz_smallEnd_z = tbz.smallEnd(zdir);

        // Get bigEnd of tileboxes
        const int tex_bigEnd_z = tex.bigEnd(zdir);
        const int tey_bigEnd_z = tey.bigEnd(zdir);
        const int tez_bigEnd_z = tez.bigEnd(zdir);
        const int tbx_bigEnd_z = tbx.bigEnd(zdir);
        const int tby_bigEnd_z = tby.bigEnd(zdir);
        const int tbz_bigEnd_z = tbz.bigEnd(zdir);

        // Box for the whole simulation domain
        amrex::Box const& domain = Geom(0).Domain();
        int const nz_domain = domain.bigEnd(zdir);

        // Set the tileboxes so that they only cover the lower/upper half of the guard cells
        constrain_tilebox_to_guards(tex, zdir, nz_domain, tex_smallEnd_z, tex_bigEnd_z);
        constrain_tilebox_to_guards(tey, zdir, nz_domain, tey_smallEnd_z, tey_bigEnd_z);
        constrain_tilebox_to_guards(tez, zdir, nz_domain, tez_smallEnd_z, tez_bigEnd_z);
        constrain_tilebox_to_guards(tbx, zdir, nz_domain, tbx_smallEnd_z, tbx_bigEnd_z);
        constrain_tilebox_to_guards(tby, zdir, nz_domain, tby_smallEnd_z, tby_bigEnd_z);
        constrain_tilebox_to_guards(tbz, zdir, nz_domain, tbz_smallEnd_z, tbz_bigEnd_z);

        amrex::ParallelFor(
            tex, Efield[0]->nComp(), [=] AMREX_GPU_DEVICE (int i, int j, int k, int icomp)
            {
                damp_field_in_guards(Ex_arr, i, j, k, icomp, zdir, nz_domain, tex_smallEnd_z, tex_bigEnd_z);
            },
            tey, Efield[1]->nComp(), [=] AMREX_GPU_DEVICE (int i, int j, int k, int icomp)
            {
                damp_field_in_guards(Ey_arr, i, j, k, icomp, zdir, nz_domain, tey_smallEnd_z, tey_bigEnd_z);
            },
            tez, Efield[2]->nComp(), [=] AMREX_GPU_DEVICE (int i, int j, int k, int icomp)
            {
                damp_field_in_guards(Ez_arr, i, j, k, icomp, zdir, nz_domain, tez_smallEnd_z, tez_bigEnd_z);
            }
        );

        amrex::ParallelFor(
            tbx, Bfield[0]->nComp(), [=] AMREX_GPU_DEVICE (int i, int j, int k, int icomp)
            {
                damp_field_in_guards(Bx_arr, i, j, k, icomp, zdir, nz_domain, tbx_smallEnd_z, tbx_bigEnd_z);
            },
            tby, Bfield[1]->nComp(), [=] AMREX_GPU_DEVICE (int i, int j, int k, int icomp)
            {
                damp_field_in_guards(By_arr, i, j, k, icomp, zdir, nz_domain, tby_smallEnd_z, tby_bigEnd_z);
            },
            tbz, Bfield[2]->nComp(), [=] AMREX_GPU_DEVICE (int i, int j, int k, int icomp)
            {
                damp_field_in_guards(Bz_arr, i, j, k, icomp, zdir, nz_domain, tbz_smallEnd_z, tbz_bigEnd_z);
            }
        );
    }
}

#ifdef WARPX_DIM_RZ
// This scales the current by the inverse volume and wraps around the depostion at negative radius.
// It is faster to apply this on the grid than to do it particle by particle.
// It is put here since there isn't another nice place for it.
void
WarpX::ApplyInverseVolumeScalingToCurrentDensity (MultiFab* Jx, MultiFab* Jy, MultiFab* Jz, int lev)
{
    const long ngJ = Jx->nGrow();
    const std::array<Real,3>& dx = WarpX::CellSize(lev);
    const Real dr = dx[0];

    constexpr int NODE = amrex::IndexType::NODE;
    AMREX_ALWAYS_ASSERT_WITH_MESSAGE(Jx->ixType().toIntVect()[0] != NODE,
        "Jr should never node-centered in r");


    for ( MFIter mfi(*Jx, TilingIfNotGPU()); mfi.isValid(); ++mfi )
    {

        Array4<Real> const& Jr_arr = Jx->array(mfi);
        Array4<Real> const& Jt_arr = Jy->array(mfi);
        Array4<Real> const& Jz_arr = Jz->array(mfi);

        Box const & tilebox = mfi.tilebox();
        Box tbr = convert( tilebox, Jx->ixType().toIntVect() );
        Box tbt = convert( tilebox, Jy->ixType().toIntVect() );
        Box tbz = convert( tilebox, Jz->ixType().toIntVect() );

        // Lower corner of tile box physical domain
        // Note that this is done before the tilebox.grow so that
        // these do not include the guard cells.
        std::array<amrex::Real,3> galilean_shift = {0,0,0};
        const std::array<Real, 3>& xyzmin = WarpX::LowerCorner(tilebox, galilean_shift, lev);
        const Real rmin  = xyzmin[0];
        const Real rminr = xyzmin[0] + (tbr.type(0) == NODE ? 0. : 0.5*dx[0]);
        const Real rmint = xyzmin[0] + (tbt.type(0) == NODE ? 0. : 0.5*dx[0]);
        const Real rminz = xyzmin[0] + (tbz.type(0) == NODE ? 0. : 0.5*dx[0]);
        const Dim3 lo = lbound(tilebox);
        const int irmin = lo.x;

        // For ishift, 1 means cell centered, 0 means node centered
        int const ishift_t = (rmint > rmin ? 1 : 0);
        int const ishift_z = (rminz > rmin ? 1 : 0);

        const int nmodes = n_rz_azimuthal_modes;

        // Grow the tileboxes to include the guard cells, except for the
        // guard cells at negative radius.
        if (rmin > 0.) {
           tbr.growLo(0, ngJ);
           tbt.growLo(0, ngJ);
           tbz.growLo(0, ngJ);
        }
        tbr.growHi(0, ngJ);
        tbt.growHi(0, ngJ);
        tbz.growHi(0, ngJ);
        tbr.grow(1, ngJ);
        tbt.grow(1, ngJ);
        tbz.grow(1, ngJ);

        // Rescale current in r-z mode since the inverse volume factor was not
        // included in the current deposition.
        amrex::ParallelFor(tbr, tbt, tbz,
        [=] AMREX_GPU_DEVICE (int i, int j, int /*k*/)
        {
            // Wrap the current density deposited in the guard cells around
            // to the cells above the axis.
            // Note that Jr(i==0) is at 1/2 dr.
            if (rmin == 0. && 0 <= i && i < ngJ) {
                Jr_arr(i,j,0,0) -= Jr_arr(-1-i,j,0,0);
            }
            // Apply the inverse volume scaling
            // Since Jr is never node centered in r, no need for distinction
            // between on axis and off-axis factors
            const amrex::Real r = amrex::Math::abs(rminr + (i - irmin)*dr);
            Jr_arr(i,j,0,0) /= (2.*MathConst::pi*r);

            for (int imode=1 ; imode < nmodes ; imode++) {
                // Wrap the current density deposited in the guard cells around
                // to the cells above the axis.
                // Note that Jr(i==0) is at 1/2 dr.
                if (rmin == 0. && 0 <= i && i < ngJ) {
                    Jr_arr(i,j,0,2*imode-1) -= Jr_arr(-1-i,j,0,2*imode-1);
                    Jr_arr(i,j,0,2*imode) -= Jr_arr(-1-i,j,0,2*imode);
                }
                // Apply the inverse volume scaling
                // Since Jr is never node centered in r, no need for distinction
                // between on axis and off-axis factors
                Jr_arr(i,j,0,2*imode-1) /= (2.*MathConst::pi*r);
                Jr_arr(i,j,0,2*imode) /= (2.*MathConst::pi*r);
            }
        },
        [=] AMREX_GPU_DEVICE (int i, int j, int /*k*/)
        {
            // Wrap the current density deposited in the guard cells around
            // to the cells above the axis.
            // If Jt is node centered, Jt[0] is located on the boundary.
            // If Jt is cell centered, Jt[0] is at 1/2 dr.
            if (rmin == 0. && 1-ishift_t <= i && i <= ngJ-ishift_t) {
                Jt_arr(i,j,0,0) -= Jt_arr(-ishift_t-i,j,0,0);
            }

            // Apply the inverse volume scaling
            // Jt is forced to zero on axis.
            const amrex::Real r = amrex::Math::abs(rmint + (i - irmin)*dr);
            if (r == 0.) {
                Jt_arr(i,j,0,0) = 0.;
            } else {
                Jt_arr(i,j,0,0) /= (2.*MathConst::pi*r);
            }

            for (int imode=1 ; imode < nmodes ; imode++) {
                // Wrap the current density deposited in the guard cells around
                // to the cells above the axis.
                if (rmin == 0. && 1-ishift_t <= i && i <= ngJ-ishift_t) {
                    Jt_arr(i,j,0,2*imode-1) -= Jt_arr(-ishift_t-i,j,0,2*imode-1);
                    Jt_arr(i,j,0,2*imode) -= Jt_arr(-ishift_t-i,j,0,2*imode);
                }

                // Apply the inverse volume scaling
                // Jt is forced to zero on axis.
                if (r == 0.) {
                    Jt_arr(i,j,0,2*imode-1) = 0.;
                    Jt_arr(i,j,0,2*imode) = 0.;
                } else {
                    Jt_arr(i,j,0,2*imode-1) /= (2.*MathConst::pi*r);
                    Jt_arr(i,j,0,2*imode) /= (2.*MathConst::pi*r);
                }
            }
        },
        [=] AMREX_GPU_DEVICE (int i, int j, int /*k*/)
        {
            // Wrap the current density deposited in the guard cells around
            // to the cells above the axis.
            // If Jz is node centered, Jt[0] is located on the boundary.
            // If Jz is cell centered, Jt[0] is at 1/2 dr.
            if (rmin == 0. && 1-ishift_z <= i && i <= ngJ-ishift_z) {
                Jz_arr(i,j,0,0) -= Jz_arr(-ishift_z-i,j,0,0);
            }

            // Apply the inverse volume scaling
            const amrex::Real r = amrex::Math::abs(rminz + (i - irmin)*dr);
            if (r == 0.) {
                // Verboncoeur JCP 164, 421-427 (2001) : corrected volume on axis
                Jz_arr(i,j,0,0) /= (MathConst::pi*dr/3.);
            } else {
                Jz_arr(i,j,0,0) /= (2.*MathConst::pi*r);
            }

            for (int imode=1 ; imode < nmodes ; imode++) {
                // Wrap the current density deposited in the guard cells around
                // to the cells above the axis.
                if (rmin == 0. && 1-ishift_z <= i && i <= ngJ-ishift_z) {
                    Jz_arr(i,j,0,2*imode-1) -= Jz_arr(-ishift_z-i,j,0,2*imode-1);
                    Jz_arr(i,j,0,2*imode) -= Jz_arr(-ishift_z-i,j,0,2*imode);
                }

                // Apply the inverse volume scaling
                if (r == 0.) {
                    // Verboncoeur JCP 164, 421-427 (2001) : corrected volume on axis
                    Jz_arr(i,j,0,2*imode-1) /= (MathConst::pi*dr/3.);
                    Jz_arr(i,j,0,2*imode) /= (MathConst::pi*dr/3.);
                } else {
                    Jz_arr(i,j,0,2*imode-1) /= (2.*MathConst::pi*r);
                    Jz_arr(i,j,0,2*imode) /= (2.*MathConst::pi*r);
                }
            }

        });
    }
}

void
WarpX::ApplyInverseVolumeScalingToChargeDensity (MultiFab* Rho, int lev)
{
    const long ngRho = Rho->nGrow();
    const std::array<Real,3>& dx = WarpX::CellSize(lev);
    const Real dr = dx[0];

    constexpr int NODE = amrex::IndexType::NODE;

    Box tilebox;

    for ( MFIter mfi(*Rho, TilingIfNotGPU()); mfi.isValid(); ++mfi )
    {

        Array4<Real> const& Rho_arr = Rho->array(mfi);

        tilebox = mfi.tilebox();
        Box tb = convert( tilebox, Rho->ixType().toIntVect() );

        // Lower corner of tile box physical domain
        // Note that this is done before the tilebox.grow so that
        // these do not include the guard cells.
        std::array<amrex::Real,3> galilean_shift = {0,0,0};
        const std::array<Real, 3>& xyzmin = WarpX::LowerCorner(tilebox, galilean_shift, lev);
        const Dim3 lo = lbound(tilebox);
        const Real rmin = xyzmin[0];
        const Real rminr = xyzmin[0] + (tb.type(0) == NODE ? 0. : 0.5*dx[0]);
        const int irmin = lo.x;
        int ishift = (rminr > rmin ? 1 : 0);

        // Grow the tilebox to include the guard cells, except for the
        // guard cells at negative radius.
        if (rmin > 0.) {
           tb.growLo(0, ngRho);
        }
        tb.growHi(0, ngRho);
        tb.grow(1, ngRho);

        // Rescale charge in r-z mode since the inverse volume factor was not
        // included in the charge deposition.
        // Note that the loop is also over ncomps, which takes care of the RZ modes,
        // as well as the old and new rho.
        amrex::ParallelFor(tb, Rho->nComp(),
        [=] AMREX_GPU_DEVICE (int i, int j, int /*k*/, int icomp)
        {
            // Wrap the charge density deposited in the guard cells around
            // to the cells above the axis.
            // Rho is located on the boundary
            if (rmin == 0. && 1-ishift <= i && i <= ngRho-ishift) {
                Rho_arr(i,j,0,icomp) -= Rho_arr(-ishift-i,j,0,icomp);
            }

            // Apply the inverse volume scaling
            const amrex::Real r = amrex::Math::abs(rminr + (i - irmin)*dr);
            if (r == 0.) {
                // Verboncoeur JCP 164, 421-427 (2001) : corrected volume on axis
                Rho_arr(i,j,0,icomp) /= (MathConst::pi*dr/3.);
            } else {
                Rho_arr(i,j,0,icomp) /= (2.*MathConst::pi*r);
            }
        });
    }
}
#endif
