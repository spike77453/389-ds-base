import pytest
import glob
from lib389.tasks import *
from lib389.utils import *
from lib389.topologies import topology_st

from lib389._constants import DEFAULT_SUFFIX, HOST_STANDALONE, PORT_STANDALONE

log = logging.getLogger(__name__)
# Skip on versions 1.4.2 and before. Rewriters are expected in 1.4.3
pytestmark = [pytest.mark.tier2,
              pytest.mark.skipif(ds_is_older('1.4.3'), reason="Not implemented")]


rewriters_container = "cn=rewriters,cn=config"

def test_rewriters_container(topology_st):
    """Test checks that rewriters container exists

    :id: 5514ae43-546b-4165-beac-896f7f0c9197
    :setup: Standalone instance
    :steps:
         1. Check container of rewriteers
    :expectedresults:
         1. Search returns only one entry
    """

    # Check container of rewriters
    ents = topology_st.standalone.search_s(rewriters_container, ldap.SCOPE_BASE, '(objectclass=*)')
    assert len(ents) == 1

    log.info('Test PASSED')

def test_foo_filter_rewriter(topology_st):
    """Test that example filter rewriter 'foo' is register and search use it

    :id: d16cf7e9-4973-4747-8694-65818156a28e
    :setup: Standalone instance
    :steps:
         1. Register foo filter rewriters
         2. Restart the server
         3. Check that the filter 'foo=foo' is rewritten into 'cn=foo'
    :expectedresults:
         1. Success
         2. Success
         3. Success
    """

    libslapd = os.path.join( topology_st.standalone.ds_paths.lib_dir, 'dirsrv/libslapd.so')
    if not os.path.exists(libslapd):
        libslapd = os.path.join( topology_st.standalone.ds_paths.lib_dir, 'dirsrv/libslapd.so.0')
    # register foo filter rewriters
    topology_st.standalone.add_s(Entry((
        "cn=foo_filter,cn=rewriters,cn=config", {
            "objectClass": "top",
            "objectClass": "extensibleObject",
            "cn": "foo_filter",
            "nsslapd-libpath": libslapd,
            "nsslapd-filterrewriter": "example_foo2cn_filter_rewriter",
        }
    )))


    topology_st.standalone.restart(60)

    # Check that the filter 'foo=foo' is rewritten into 'cn=foo'
    ents = topology_st.standalone.search_s(rewriters_container, ldap.SCOPE_SUBTREE, '(foo=foo_filter)')
    assert len(ents) > 0
    assert ents[0].dn == "cn=foo_filter,cn=rewriters,cn=config"

    log.info('Test PASSED')

