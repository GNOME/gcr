gi_docgen = find_program('gi-docgen')

subdir('gck')
subdir('gcr')
gtk_docs_targets = []
if get_option('gtk3')
  subdir('gcr-gtk3')
  gtk_docs_targets += gcr_gtk3_docs
endif
if get_option('gtk4')
  subdir('gcr-gtk4')
  gtk_docs_targets += gcr_gtk4_docs
endif

# Create a pseudo target that build all docs at once
alias_target('docs',
  gck_docs,
  gcr_docs,
  gtk_docs_targets,
)
