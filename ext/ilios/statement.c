#include "ilios.h"

static void statement_destroy(void *ptr);
static size_t statement_memsize(const void *ptr);

const rb_data_type_t cassandra_statement_data_type = {
    "Ilios::Cassandra::Statement",
    {
        NULL,
        statement_destroy,
        statement_memsize,
#ifdef HAVE_RB_GC_MARK_MOVABLE
        NULL,
#endif
    },
    0, 0,
    RUBY_TYPED_FREE_IMMEDIATELY | RUBY_TYPED_FROZEN_SHAREABLE,
};

static void statement_destroy(void *ptr)
{
    CassandraStatement *cassandra_statement = (CassandraStatement *)ptr;

    if (cassandra_statement->prepared) {
        cass_prepared_free(cassandra_statement->prepared);
    }
    if (cassandra_statement->statement) {
        cass_statement_free(cassandra_statement->statement);
    }
    cassandra_statement->prepared = NULL;
    cassandra_statement->statement = NULL;
}

static size_t statement_memsize(const void *ptr)
{
    return sizeof(CassandraStatement);
}

void Init_statement(void)
{
    rb_undef_alloc_func(cStatement);
}
