file(REMOVE_RECURSE
  "libeshamell.a"
  "libeshamell.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/eshamell.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
