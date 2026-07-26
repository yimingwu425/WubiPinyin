target("WubiPinyinCore")
  set_kind("static")
  set_languages("c++20")
  add_files("./*.cpp")
  add_includedirs(".", {public = true})
  if is_plat("windows") then
    add_links("winsqlite3", {public = true})
  else
    add_requires("sqlite3")
    add_packages("sqlite3", {public = true})
  end
