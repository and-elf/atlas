# demo-physics_observer (issue #188): the minimal downstream capability that
# consumes rigid_body's own BodyState property (consumes: [BodyState], not a
# depends_on: [rigid_body] entry) - see this module's own
# physics_observer.hpp doc comment.
atlas_add_demo_capability(NAME physics_observer SOURCES physics_observer.cpp CAPABILITY_DEPENDS rigid_body)
