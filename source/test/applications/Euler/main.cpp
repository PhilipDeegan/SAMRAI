/*************************************************************************
 *
 * This file is part of the SAMRAI distribution.  For full copyright
 * information, see COPYRIGHT and LICENSE.
 *
 * Copyright:     (c) 1997-2023 Lawrence Livermore National Security, LLC
 * Description:   Main program for SAMRAI Euler gas dynamics sample application
 *
 ************************************************************************/

#include "SAMRAI/SAMRAI_config.h"

// Header for application-specific algorithm/data structure object

#include "Euler.h"

// Headers for major algorithm/data structure objects from SAMRAI

#include "SAMRAI/mesh/BergerRigoutsos.h"
#include "SAMRAI/geom/CartesianGridGeometry.h"
#include "SAMRAI/mesh/GriddingAlgorithm.h"
#include "SAMRAI/mesh/TreeLoadBalancer.h"
#include "SAMRAI/algs/HyperbolicLevelIntegrator.h"
#include "SAMRAI/hier/PatchHierarchy.h"
#include "SAMRAI/mesh/StandardTagAndInitialize.h"
#include "SAMRAI/algs/TimeRefinementIntegrator.h"
#include "SAMRAI/algs/TimeRefinementLevelStrategy.h"
#include "SAMRAI/appu/VisItDataWriter.h"

// Headers for basic SAMRAI objects

#include "SAMRAI/hier/BoxContainer.h"
#include "SAMRAI/hier/Index.h"
#include "SAMRAI/hier/PatchLevel.h"
#include "SAMRAI/hier/VariableDatabase.h"
#include "SAMRAI/tbox/BalancedDepthFirstTree.h"
#include "SAMRAI/tbox/Database.h"
#include "SAMRAI/tbox/SiloDatabaseFactory.h"
#include "SAMRAI/tbox/InputDatabase.h"
#include "SAMRAI/tbox/InputManager.h"
#include "SAMRAI/tbox/SAMRAI_MPI.h"
#include "SAMRAI/tbox/SAMRAIManager.h"
#include "SAMRAI/tbox/PIO.h"
#include "SAMRAI/tbox/RestartManager.h"
#include "SAMRAI/tbox/Utilities.h"
#include "SAMRAI/tbox/Timer.h"
#include "SAMRAI/tbox/TimerManager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <fstream>
#include <memory>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#include <sys/stat.h>

#ifdef _OPENMP
#include <omp.h>
#endif

// Classes for autotesting.

#if (TESTING == 1)
#include "test/testlib/AutoTester.h"
#endif

using namespace SAMRAI;

/*
 ************************************************************************
 *
 * This is the main program for an AMR Euler gas dynamics application
 * built using SAMRAI.   The application program is constructed by
 * composing a variety of algorithm objects found in SAMRAI plus some
 * others that are specific to this application.   The following brief
 * discussion summarizes these objects.
 *
 *    hier::PatchHierarchy - A container for the AMR patch hierarchy and
 *       the data on the grid.
 *
 *    geom::CartesianGridGeometry - Defines and maintains the Cartesian
 *       coordinate system on the grid.  The hier::PatchHierarchy
 *       maintains a reference to this object.
 *
 * A single overarching algorithm object drives the time integration
 * and adaptive gridding processes:
 *
 *    algs::TimeRefinementIntegrator - Coordinates time integration and
 *       adaptive gridding procedures for the various levels
 *       in the AMR patch hierarchy.  Local time refinement is
 *       employed during hierarchy integration; i.e., finer
 *       levels are advanced using smaller time increments than
 *       coarser level.  Thus, this object also invokes data
 *       synchronization procedures which couple the solution on
 *       different patch hierarchy levels.
 *
 * The time refinement integrator is not specific to the numerical
 * methods used and the problem being solved.   It maintains references
 * to two other finer grain algorithmic objects, more specific to
 * the problem at hand, with which it is configured when they are
 * passed into its constructor.   They are:
 *
 *    algs::HyperbolicLevelIntegrator - Defines data management procedures
 *       for level integration, data synchronization between levels,
 *       and tagging cells for refinement.  These operations are
 *       tailored to explicit time integration algorithms used for
 *       hyperbolic systems of conservation laws, such as the Euler
 *       equations.  This integrator manages data for numerical
 *       routines that treat individual patches in the AMR patch
 *       hierarchy.  In this particular application, it maintains a
 *       pointer to the Euler object that defines variables and
 *       provides numerical routines for the Euler model.
 *
 *       Euler - Defines variables and numerical routines for the
 *          discrete Euler equations on each patch in the AMR
 *          hierarchy.
 *
 *    mesh::GriddingAlgorithm - Drives the AMR patch hierarchy generation
 *       and regridding procedures.  This object maintains
 *       references to three other algorithmic objects with
 *       which it is configured when they are passed into its
 *       constructor.   They are:
 *
 *       mesh::BergerRigoutsos - Clusters cells tagged for refinement on a
 *          patch level into a collection of logically-rectangular
 *          box domains.
 *
 *       mesh::TreeLoadBalancer - Processes the boxes generated by the
 *          mesh::BergerRigoutsos algorithm into a configuration from
 *          which patches are contructed.  The algorithm we use in this
 *          class assumes a spatially-uniform workload distribution;
 *          thus, it attempts to produce a collection of boxes
 *          each of which contains the same number of cells.  The
 *          load balancer also assigns patches to processors.
 *
 *       mesh::StandardTagAndInitialize - Couples the gridding algorithm
 *          to the HyperbolicIntegrator. Selects cells for
 *          refinement based on either Gradient detection, Richardson
 *          extrapolation, or pre-defined Refine box region.  The
 *          object maintains a pointer to the algs::HyperbolicLevelIntegrator,
 *          which is passed into its constructor, for this purpose.
 *
 ************************************************************************
 */

/*
 *******************************************************************
 *
 * For each run, the input filename and restart information
 * (if needed) must be given on the command line.
 *
 *      For non-restarted case, command line is:
 *
 *          executable <input file name>
 *
 *      For restarted run, command line is:
 *
 *          executable <input file name> <restart directory> \
 *                     <restart number>
 *
 * Accessory routines used within the main program:
 *
 *   dumpVizData1dPencil - Writes 1d pencil of Euler solution data
 *      to plot files so that it may be viewed in MatLab.  This
 *      routine assumes a single patch level in 2d and 3d.  In
 *      other words, it only plots data on level zero.  It can
 *      handle AMR in 1d.
 *
 *******************************************************************
 */

static void
dumpMatlabData1dPencil(
   const std::string& dirname,
   const std::string& filename,
   const int ext,
   const double plot_time,
   const std::shared_ptr<hier::PatchHierarchy> hierarchy,
   const tbox::Dimension::dir_t pencil_direction,
   const bool default_pencil,
   const std::vector<int>& pencil_index,
   Euler* euler_model);

int main(
   int argc,
   char* argv[])
{

   /*
    * Initialize tbox::MPI and SAMRAI, enable logging, and process command line.
    */

   tbox::SAMRAI_MPI::init(&argc, &argv);
   tbox::SAMRAIManager::initialize();
   tbox::SAMRAIManager::startup();
   const tbox::SAMRAI_MPI& mpi(tbox::SAMRAI_MPI::getSAMRAIWorld());

   int num_failures = 0;

   {

      std::string input_filename;
      std::string restart_read_dirname;
      int restore_num = 0;

      bool is_from_restart = false;

      if ((argc != 2) && (argc != 4)) {
         tbox::pout << "USAGE:  " << argv[0] << " <input filename> "
                    << "<restart dir> <restore number> [options]\n"
                    << "  options:\n"
                    << "  none at this time"
                    << std::endl;
         tbox::SAMRAI_MPI::abort();
         return -1;
      } else {
         input_filename = argv[1];
         if (argc == 4) {
            restart_read_dirname = argv[2];
            restore_num = atoi(argv[3]);

            is_from_restart = true;
         }
      }

      tbox::plog << "input_filename = " << input_filename << std::endl;
      tbox::plog << "restart_read_dirname = " << restart_read_dirname << std::endl;
      tbox::plog << "restore_num = " << restore_num << std::endl;

      /*
       * Create input database and parse all data in input file.
       */

      std::shared_ptr<tbox::InputDatabase> input_db(
         new tbox::InputDatabase("input_db"));
      tbox::InputManager::getManager()->parseInputFile(input_filename, input_db);

      /*
       * Retrieve "GlobalInputs" section of the input database and set
       * values accordingly.
       */

      if (input_db->keyExists("GlobalInputs")) {
         std::shared_ptr<tbox::Database> global_db(
            input_db->getDatabase("GlobalInputs"));

#ifdef SGS
         // TODO change to what?
         if (global_db->keyExists("tag_clustering_method")) {
            std::string tag_clustering_method =
               global_db->getString("tag_clustering_method");
            mesh::BergerRigoutsos::setClusteringOption(tag_clustering_method);
         }
#endif

         if (global_db->keyExists("call_abort_in_serial_instead_of_exit")) {
            bool flag = global_db->
               getBool("call_abort_in_serial_instead_of_exit");
            tbox::SAMRAI_MPI::setCallAbortInSerialInsteadOfExit(flag);
         }
      }

      /*
       * Retrieve "Main" section of the input database.  First, read dump
       * information, which is used for writing plot files.  Second, if
       * proper restart information was given on command line, and the
       * restart interval is non-zero, create a restart database.
       */

      std::shared_ptr<tbox::Database> main_db(
         input_db->getDatabase("Main"));

      const tbox::Dimension dim(static_cast<unsigned short>(main_db->getInteger("dim")));

      const std::string base_name =
         main_db->getStringWithDefault("base_name", "unnamed");

      const std::string log_filename =
         main_db->getStringWithDefault("log_filename", base_name + ".log");

      bool log_all_nodes = false;
      if (main_db->keyExists("log_all_nodes")) {
         log_all_nodes = main_db->getBool("log_all_nodes");
      }
      if (log_all_nodes) {
         tbox::PIO::logAllNodes(log_filename);
      } else {
         tbox::PIO::logOnlyNodeZero(log_filename);
      }

#ifdef _OPENMP
      tbox::plog << "Compiled with OpenMP version " << _OPENMP
                 << ".  Running with " << omp_get_max_threads() << " threads."
                 << std::endl;
#else
      tbox::plog << "Compiled without OpenMP.\n";
#endif

      int viz_dump_interval = 0;
      if (main_db->keyExists("viz_dump_interval")) {
         viz_dump_interval = main_db->getInteger("viz_dump_interval");
      }

      const std::string visit_dump_dirname =
         main_db->getStringWithDefault("viz_dump_dirname", base_name + ".visit");

      int visit_number_procs_per_file = 1;
      if (viz_dump_interval > 0) {
         if (main_db->keyExists("visit_number_procs_per_file")) {
            visit_number_procs_per_file =
               main_db->getInteger("visit_number_procs_per_file");
         }
      }

      std::string matlab_dump_filename;
      std::string matlab_dump_dirname;
      int matlab_dump_interval = 0;
      tbox::Dimension::dir_t matlab_pencil_direction = 0;
      std::vector<int> matlab_pencil_index(dim.getValue() - 1);
      bool matlab_default_pencil = true;
      for (int id = 0; id < dim.getValue() - 1; ++id) {
         matlab_pencil_index[id] = 0;
      }

      if (main_db->keyExists("matlab_dump_interval")) {
         matlab_dump_interval = main_db->getInteger("matlab_dump_interval");
      }
      if (matlab_dump_interval > 0) {
         if (main_db->keyExists("matlab_dump_filename")) {
            matlab_dump_filename = main_db->getString("matlab_dump_filename");
         }
         if (main_db->keyExists("matlab_dump_dirname")) {
            matlab_dump_dirname = main_db->getString("matlab_dump_dirname");
         }
         if (main_db->keyExists("matlab_pencil_direction")) {
            matlab_pencil_direction =
               static_cast<tbox::Dimension::dir_t>(main_db->getInteger("matlab_pencil_direction"));
         }
         if (main_db->keyExists("matlab_pencil_index")) {
            matlab_default_pencil = false;
            matlab_pencil_index =
               main_db->getIntegerVector("matlab_pencil_index");
            if (static_cast<int>(matlab_pencil_index.size()) != dim.getValue() - 1) {
               TBOX_ERROR("`matlab_pencil_index' has "
                  << matlab_pencil_index.size() << " values in input. "
                  << dim.getValue() - 1 << " values must be specified when default"
                  << " is overridden.");
            }
         }
      }

      int restart_interval = 0;
      if (main_db->keyExists("restart_interval")) {
         restart_interval = main_db->getInteger("restart_interval");
      }

      const std::string restart_write_dirname =
         main_db->getStringWithDefault("restart_write_dirname",
            base_name + ".restart");

      bool use_refined_timestepping = true;
      if (main_db->keyExists("timestepping")) {
         std::string timestepping_method = main_db->getString("timestepping");
         if (timestepping_method == "SYNCHRONIZED") {
            use_refined_timestepping = false;
         }
      }

#if (TESTING == 1) && !defined(HAVE_HDF5)
      /*
       * If we are autotesting on a system w/o HDF5, the read from
       * restart will result in an error.  We want this to happen
       * for users, so they know there is a problem with the restart,
       * but we don't want it to happen when autotesting.
       */
      is_from_restart = false;
      restart_interval = 0;
#endif

      const bool write_restart = (restart_interval > 0)
         && !(restart_write_dirname.empty());

      /*
       * Get restart manager and root restart database.  If run is from
       * restart, open the restart file.
       */

      tbox::RestartManager* restart_manager = tbox::RestartManager::getManager();

#ifdef HAVE_SILO
      /*
       * If SILO is present then use SILO as the file storage format
       * for this example, otherwise it will default to HDF5.
       */
      std::shared_ptr<tbox::SiloDatabaseFactory> silo_database_factory(
         new tbox::SiloDatabaseFactory());
      restart_manager->setDatabaseFactory(silo_database_factory);
#endif

      if (is_from_restart) {
         restart_manager->
         openRestartFile(restart_read_dirname, restore_num,
            mpi.getSize());
      }

      /*
       * Setup the timer manager to trace timing statistics during execution
       * of the code.  The list of timers is given in the tbox::TimerManager
       * section of the input file.  Timing information is stored in the
       * restart file.  Timers will automatically be initialized to their
       * previous state if the run is restarted, unless they are explicitly
       * reset using the tbox::TimerManager::resetAllTimers() routine.
       */

      tbox::TimerManager::createManager(input_db->getDatabase("TimerManager"));

      /*
       * Create major algorithm and data objects which comprise application.
       * Each object is initialized either from input data or restart
       * files, or a combination of both.  Refer to each class constructor
       * for details.  For more information on the composition of objects
       * and the roles they play in this application, see comments at top of file.
       */

      std::shared_ptr<geom::CartesianGridGeometry> grid_geometry(
         new geom::CartesianGridGeometry(
            dim,
            "CartesianGeometry",
            input_db->getDatabase("CartesianGeometry")));

      std::shared_ptr<hier::PatchHierarchy> patch_hierarchy(
         new hier::PatchHierarchy(
            "PatchHierarchy",
            grid_geometry,
            input_db->getDatabase("PatchHierarchy")));

      Euler* euler_model = new Euler("Euler",
            dim,
            input_db->getDatabase("Euler"),
            grid_geometry);

      std::shared_ptr<algs::HyperbolicLevelIntegrator> hyp_level_integrator(
         new algs::HyperbolicLevelIntegrator(
            "HyperbolicLevelIntegrator",
            input_db->getDatabase("HyperbolicLevelIntegrator"),
            euler_model,
            use_refined_timestepping));

      std::shared_ptr<mesh::StandardTagAndInitialize> error_detector(
         new mesh::StandardTagAndInitialize(
            "StandardTagAndInitialize",
            hyp_level_integrator.get(),
            input_db->getDatabase("StandardTagAndInitialize")));

      std::shared_ptr<mesh::BergerRigoutsos> box_generator(
         new mesh::BergerRigoutsos(
            dim,
            input_db->getDatabaseWithDefault(
               "BergerRigoutsos",
               std::shared_ptr<tbox::Database>())));

      std::shared_ptr<mesh::TreeLoadBalancer> load_balancer(
         new mesh::TreeLoadBalancer(
            dim,
            "LoadBalancer",
            input_db->getDatabase("LoadBalancer"),
            std::shared_ptr<tbox::RankTreeStrategy>(new tbox::BalancedDepthFirstTree)));
      load_balancer->setSAMRAI_MPI(
         tbox::SAMRAI_MPI::getSAMRAIWorld());

      std::shared_ptr<mesh::GriddingAlgorithm> gridding_algorithm(
         new mesh::GriddingAlgorithm(
            patch_hierarchy,
            "GriddingAlgorithm",
            input_db->getDatabase("GriddingAlgorithm"),
            error_detector,
            box_generator,
            load_balancer));

      std::shared_ptr<algs::TimeRefinementIntegrator> time_integrator(
         new algs::TimeRefinementIntegrator(
            "TimeRefinementIntegrator",
            input_db->getDatabase("TimeRefinementIntegrator"),
            patch_hierarchy,
            hyp_level_integrator,
            gridding_algorithm));

      /*
       * Set up Visualization writer(s).  Note that the Euler application
       * creates some derived data quantities so we register the Euler model
       * as a derived data writer.  If no derived data is written, this step
       * is not necessary.
       */
#ifdef HAVE_HDF5
      std::shared_ptr<appu::VisItDataWriter> visit_data_writer(
         new appu::VisItDataWriter(
            dim,
            "Euler VisIt Writer",
            visit_dump_dirname,
            visit_number_procs_per_file));
      euler_model->registerVisItDataWriter(visit_data_writer);
#endif

      /*
       * Initialize hierarchy configuration and data on all patches.
       * Then, close restart file and write initial state for visualization.
       */

      double dt_now = time_integrator->initializeHierarchy();

      tbox::RestartManager::getManager()->closeRestartFile();

#if (TESTING == 1)
      /*
       * Create the autotesting component which will verify correctness
       * of the problem. If no automated testing is done, the object does
       * not get used.
       */
      AutoTester autotester("AutoTester", dim, input_db);
#endif

      /*
       * After creating all objects and initializing their state, we
       * print the input database and variable database contents
       * to the log file.
       */

#if 1
      tbox::plog << "\nCheck input data and variables before simulation:"
                 << std::endl;
      tbox::plog << "Input database..." << std::endl;
      input_db->printClassData(tbox::plog);
      tbox::plog << "\nVariable database..." << std::endl;
      hier::VariableDatabase::getDatabase()->printClassData(tbox::plog);

#endif
      tbox::plog << "\nCheck Euler data... " << std::endl;
      euler_model->printClassData(tbox::plog);

      /*
       * Create timers for measuring I/O.
       */
      std::shared_ptr<tbox::Timer> t_write_viz(
         tbox::TimerManager::getManager()->getTimer("apps::main::write_viz"));
      std::shared_ptr<tbox::Timer> t_write_restart(
         tbox::TimerManager::getManager()->getTimer(
            "apps::main::write_restart"));

      t_write_viz->start();
      if (matlab_dump_interval > 0) {
         dumpMatlabData1dPencil(matlab_dump_dirname,
            matlab_dump_filename,
            time_integrator->getIntegratorStep(),
            time_integrator->getIntegratorTime(),
            patch_hierarchy,
            matlab_pencil_direction,
            matlab_default_pencil,
            matlab_pencil_index,
            euler_model);
      }
#ifdef HAVE_HDF5
      if (viz_dump_interval > 0) {

         visit_data_writer->writePlotData(
            patch_hierarchy,
            time_integrator->getIntegratorStep(),
            time_integrator->getIntegratorTime());
      }
#endif
      t_write_viz->stop();

      /*
       * Time step loop.  Note that the step count and integration
       * time are maintained by algs::TimeRefinementIntegrator.
       */

      double loop_time = time_integrator->getIntegratorTime();
      double loop_time_end = time_integrator->getEndTime();

#if (TESTING == 1)
      /*
       * If we are doing autotests, check result...
       */
      num_failures += autotester.evalTestData(
            time_integrator->getIntegratorStep(),
            patch_hierarchy,
            time_integrator,
            hyp_level_integrator,
            gridding_algorithm);
#endif

      while ((loop_time < loop_time_end) &&
             time_integrator->stepsRemaining()) {

         int iteration_num = time_integrator->getIntegratorStep() + 1;

         tbox::pout << "++++++++++++++++++++++++++++++++++++++++++++" << std::endl;
         tbox::pout << "At begining of timestep # " << iteration_num - 1
                    << std::endl;
         tbox::pout << "Simulation time is " << loop_time << std::endl;
         tbox::pout << "Current dt is " << dt_now << std::endl;

         double dt_new = time_integrator->advanceHierarchy(dt_now);

         loop_time += dt_now;
         dt_now = dt_new;

         tbox::pout << "At end of timestep # " << iteration_num - 1 << std::endl;
         tbox::pout << "Simulation time is " << loop_time << std::endl;
         tbox::pout << "++++++++++++++++++++++++++++++++++++++++++++" << std::endl;

         /*
          * At specified intervals, write restart files.
          */
         if (write_restart) {

            if ((iteration_num % restart_interval) == 0) {
               t_write_restart->start();
               tbox::RestartManager::getManager()->
               writeRestartFile(restart_write_dirname,
                  iteration_num);
               t_write_restart->stop();
            }
         }

         /*
          * At specified intervals, write out data files for plotting.
          */
         t_write_viz->start();
#ifdef HAVE_HDF5
         if ((viz_dump_interval > 0)
             && (iteration_num % viz_dump_interval) == 0) {
            visit_data_writer->writePlotData(patch_hierarchy,
               iteration_num,
               loop_time);
         }
#endif
         if ((matlab_dump_interval > 0)
             && (iteration_num % matlab_dump_interval) == 0) {
            dumpMatlabData1dPencil(matlab_dump_dirname,
               matlab_dump_filename,
               iteration_num,
               loop_time,
               patch_hierarchy,
               matlab_pencil_direction,
               matlab_default_pencil,
               matlab_pencil_index,
               euler_model);
         }
         t_write_viz->stop();

#if (TESTING == 1)
         /*
          * If we are doing autotests, check result...
          */
         num_failures += autotester.evalTestData(iteration_num,
               patch_hierarchy,
               time_integrator,
               hyp_level_integrator,
               gridding_algorithm);
#endif

         /*
          * Write byte transfer information to log file.
          */
#if 0
         char num_buf[8];
         snprintf(num_buf, 8, "%02d", iteration_num);
         tbox::plog << "Step " << num_buf
                    << " P" << mpi.getRank()
                    << ": " << tbox::SAMRAI_MPI::getIncomingBytes()
                    << " bytes in" << std::endl;
#endif

      }

      tbox::plog << "GriddingAlgorithm statistics:\n";
      gridding_algorithm->printStatistics();

      /*
       * Output timer results.
       */
      tbox::TimerManager::getManager()->print(tbox::plog);

      /*
       * At conclusion of simulation, deallocate objects.
       */
      patch_hierarchy.reset();
      grid_geometry.reset();

      box_generator.reset();
      load_balancer.reset();
      hyp_level_integrator.reset();
      error_detector.reset();
      gridding_algorithm.reset();
      time_integrator.reset();
#ifdef HAVE_HDF5
      visit_data_writer.reset();
#endif

      if (euler_model) delete euler_model;

   }

   if (num_failures == 0) {
      tbox::pout << "\nPASSED:  Euler" << std::endl;
   }

   tbox::SAMRAIManager::shutdown();
   tbox::SAMRAIManager::finalize();
   tbox::SAMRAI_MPI::finalize();

   return num_failures;
}

static void dumpMatlabData1dPencil(
   const std::string& dirname,
   const std::string& filename,
   const int ext,
   const double plot_time,
   const std::shared_ptr<hier::PatchHierarchy> hierarchy,
   const tbox::Dimension::dir_t pencil_direction,
   const bool default_pencil,
   const std::vector<int>& pencil_index,
   Euler* euler_model)
{
   const tbox::SAMRAI_MPI& mpi(tbox::SAMRAI_MPI::getSAMRAIWorld());

   /*
    * Compute the boxes to write out data at each level of the hierarchy.
    */

   const tbox::Dimension& dim = hierarchy->getDim();

   int nlevels = 1;

   if ((dim == tbox::Dimension(1))) {
      nlevels = hierarchy->getNumberOfLevels();
   }

   hier::BoxContainer domain(hierarchy->getGridGeometry()->getPhysicalDomain());
   hier::Box pencil_box(domain.getBoundingBox());

   if (dim > tbox::Dimension(1)) {
      int indx = 0;
      std::vector<int> tmp(dim.getValue() - 1);
      for (tbox::Dimension::dir_t id = 0; id < dim.getValue() - 1; ++id) {
         tmp[id] = pencil_index[id];
      }
      if (default_pencil) {
         hier::Index ifirst = domain.getBoundingBox().lower();
         indx = 0;
         for (tbox::Dimension::dir_t id = 0; id < dim.getValue(); ++id) {
            if (id != pencil_direction) {
               tmp[indx] = ifirst(id);
               ++indx;
            }
         }
      }
      indx = 0;
      for (tbox::Dimension::dir_t id = 0; id < dim.getValue(); ++id) {
         if (id != pencil_direction) {
            pencil_box.setLower(id, tmp[indx]);
            pencil_box.setUpper(id, tmp[indx]);
            ++indx;
         }
      }
   }

   std::vector<hier::BoxContainer> outboxes(nlevels);

   for (int l1 = 0; l1 < nlevels; ++l1) {
      std::shared_ptr<hier::PatchLevel> level(hierarchy->getPatchLevel(l1));
      outboxes[l1] = hier::BoxContainer(level->getBoxes());

      if (l1 < nlevels - 1) {

         std::shared_ptr<hier::PatchLevel> finer_level(
            hierarchy->getPatchLevel(l1 + 1));
         hier::IntVector coarsen_ratio =
            finer_level->getRatioToCoarserLevel();
         hier::BoxContainer takeaway(finer_level->getBoxes());
         takeaway.coarsen(coarsen_ratio);
         outboxes[l1].removeIntersections(takeaway);
      }

   }

   /*
    * Create matlab filename and open the output stream.
    */

   std::string dump_filename = filename;
   if (!dirname.empty()) {
      tbox::Utilities::recursiveMkdir(dirname);
      dump_filename = dirname;
      dump_filename += "/";
      dump_filename += filename;
   }

   const int size = static_cast<int>(dump_filename.length()) + 16;
   char* buffer = new char[size];

   if (mpi.getSize() > 1) {
      snprintf(buffer, size, "%s.%04d.dat.%05d", dump_filename.c_str(),
         ext, mpi.getRank());
   } else {
      snprintf(buffer, size, "%s_%04d.dat", dump_filename.c_str(), ext);
   }

   /*
    * Open a new output file having name name of buffer character array.
    */

   std::ofstream outfile(buffer, std::ios::out);
   outfile.setf(std::ios::scientific);
   outfile.precision(10);

   delete[] buffer;

   /*
    * There are 7 values dumped for every cell.  Here we dump the time.
    */
   for (int i = 0; i < 6 + 1; ++i) {
      outfile << plot_time << "  ";
   }
   outfile << std::endl;

   euler_model->setDataContext(
      hier::VariableDatabase::getDatabase()->getContext("CURRENT"));

   for (int l5 = 0; l5 < nlevels; ++l5) {
      std::shared_ptr<hier::PatchLevel> level(hierarchy->getPatchLevel(l5));

      hier::Box level_pencil_box = pencil_box;
      if (l5 > 0) {
         level_pencil_box.refine(level->getRatioToLevelZero());
      }

      for (hier::PatchLevel::iterator i(level->begin());
           i != level->end(); ++i) {
         const std::shared_ptr<hier::Patch>& patch = *i;
         hier::Box pbox = patch->getBox();

         for (hier::BoxContainer::iterator b = outboxes[l5].begin();
              b != outboxes[l5].end(); ++b) {
            const hier::Box box = (*b) * pbox * level_pencil_box;

            euler_model->writeData1dPencil(patch,
               box,
               pencil_direction,
               outfile);
         }

      }

   }

   euler_model->clearDataContext();

   outfile.close();

}
