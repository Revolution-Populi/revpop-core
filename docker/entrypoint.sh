#!/bin/bash
REVPOPD="/usr/local/bin/witness_node"

# For blockchain download
VERSION=`cat /etc/revpop/version`

## Supported Environmental Variables
#
#   * $REVPOPD_SEED_NODES
#   * $REVPOPD_RPC_ENDPOINT
#   * $REVPOPD_PLUGINS
#   * $REVPOPD_REPLAY
#   * $REVPOPD_RESYNC
#   * $REVPOPD_P2P_ENDPOINT
#   * $REVPOPD_WITNESS_ID
#   * $REVPOPD_PRIVATE_KEY
#   * $REVPOPD_TRACK_ACCOUNTS
#   * $REVPOPD_PARTIAL_OPERATIONS
#   * $REVPOPD_MAX_OPS_PER_ACCOUNT
#   * $REVPOPD_ES_NODE_URL
#   * $REVPOPD_ES_START_AFTER_BLOCK
#   * $REVPOPD_TRUSTED_NODE
#

ARGS=""
# Prevent minority fork error
ARGS+=" --required-participation 0"
# Translate environmental variables
if [[ ! -z "$REVPOPD_SEED_NODES" ]]; then
    for NODE in $REVPOPD_SEED_NODES ; do
        ARGS+=" --seed-node=$NODE"
    done
fi
if [[ ! -z "$REVPOPD_RPC_ENDPOINT" ]]; then
    ARGS+=" --rpc-endpoint=${REVPOPD_RPC_ENDPOINT}"
fi

if [[ ! -z "$REVPOPD_REPLAY" ]]; then
    ARGS+=" --replay-blockchain"
fi

if [[ ! -z "$REVPOPD_RESYNC" ]]; then
    ARGS+=" --resync-blockchain"
fi

if [[ ! -z "$REVPOPD_P2P_ENDPOINT" ]]; then
    ARGS+=" --p2p-endpoint=${REVPOPD_P2P_ENDPOINT}"
fi

if [[ ! -z "$REVPOPD_WITNESS_ID" ]]; then
    ARGS+=" --witness-id=$REVPOPD_WITNESS_ID"
fi

if [[ ! -z "$REVPOPD_PRIVATE_KEY" ]]; then
    ARGS+=" --private-key=$REVPOPD_PRIVATE_KEY"
fi

if [[ ! -z "$REVPOPD_TRACK_ACCOUNTS" ]]; then
    for ACCOUNT in $REVPOPD_TRACK_ACCOUNTS ; do
        ARGS+=" --track-account=$ACCOUNT"
    done
fi

if [[ ! -z "$REVPOPD_PARTIAL_OPERATIONS" ]]; then
    ARGS+=" --partial-operations=${REVPOPD_PARTIAL_OPERATIONS}"
fi

if [[ ! -z "$REVPOPD_MAX_OPS_PER_ACCOUNT" ]]; then
    ARGS+=" --max-ops-per-account=${REVPOPD_MAX_OPS_PER_ACCOUNT}"
fi

if [[ ! -z "$REVPOPD_ES_NODE_URL" ]]; then
    ARGS+=" --elasticsearch-node-url=${REVPOPD_ES_NODE_URL}"
fi

if [[ ! -z "$REVPOPD_ES_START_AFTER_BLOCK" ]]; then
    ARGS+=" --elasticsearch-start-es-after-block=${REVPOPD_ES_START_AFTER_BLOCK}"
fi

if [[ ! -z "$REVPOPD_TRUSTED_NODE" ]]; then
    ARGS+=" --trusted-node=${REVPOPD_TRUSTED_NODE}"
fi

## Link the revpop config file into home
## This link has been created in Dockerfile, already
ln -f -s /etc/revpop/config.ini /var/lib/revpop
ln -f -s /etc/revpop/logging.ini /var/lib/revpop

# Plugins need to be provided in a space-separated list, which
# makes it necessary to write it like this
if [[ ! -z "$REVPOPD_PLUGINS" ]]; then
   exec "$REVPOPD" --data-dir "${HOME}" ${ARGS} ${REVPOPD_ARGS} --plugins "${REVPOPD_PLUGINS}"
else
   exec "$REVPOPD" --data-dir "${HOME}" ${ARGS} ${REVPOPD_ARGS}
fi
