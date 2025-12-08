project('mercury', 'c')

cjson = dependency('libcjson')

add_project_arguments(
  ['-Wall'],
  language: 'c'
)
executable(
  'mercury', 
  'mercury.c', 
  'gameserver.c', 
  dependencies: [cjson],
)

