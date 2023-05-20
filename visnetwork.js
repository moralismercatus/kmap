/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/

class Network
{
    network = null;

    constructor( container )
    {
        try
        {
            // TODO: Do better error checking than assertions.
            console.assert( container );

            let edges = new vis.DataSet();
            let nodes = new vis.DataSet();
            let data = { nodes: nodes
                       , edges: edges };
            //  TODO: All these options should exist in a setting node, configurable by the user.
            let options = { height: '100%'
                          , width: '100%'
                          , autoResize: true
                          , layout: { hierarchical: { direction: 'LR'
                                                    // , sortMethod: 'directed' Disabled this setting, as enabling it caused all leaf nodes to become aligned.
                                                    , nodeSpacing: 50
                                                    , levelSeparation: 250
                                                    , blockShifting: true
                                                    , edgeMinimization: true
                                                    , parentCentralization: true } }
                          , physics: { enabled: false
                                     , hierarchicalRepulsion: { nodeDistance: 1 } }
                          , interaction: { dragNodes: false }
                          , edges: { smooth: { type: 'continuous'
                                             , forceDirection: 'horizontal' } }
                          , nodes: { shape: 'box'
                                   , shapeProperties: { interpolation: false }
                                   , widthConstraint: { minimum: 200
                                                      , maximum: 200 }
                                   , font: { multi: 'html' } } };

            this.network = new vis.Network( container, data, options );
            
            // Presumably it's safe to register these event handlers before the event system is set up, so long as they don't fire prior to its set up.
            this.network.canvas.frame.onkeydown = network_onkeydown;
            this.network.canvas.frame.onkeyup = network_onkeyup;
            this.network.on( 'zoom', function( ev ){ kmap.event_store().fire_event( to_VectorString( [ 'subject.network', 'verb.scaled', 'object.viewport' ] ) ).throw_on_error(); } );
            this.network.on( 'deselectNode', network_on_deselect_node_event( this ) );
            this.network.on( 'selectNode', network_on_select_node_event( this ) );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    add_edge( edge_id
            , from_id
            , to_id )
    {
        try
        {
            this.network.body.data.edges.add( { id: edge_id
                                              , from: from_id
                                              , to: to_id } );
        }
        catch( err )
        {
            console.log( String( err ) );
            throw err;
        }
    }

    // TODO: Ensure that from_ids.size() == to_ids.size()
    add_edges( edge_ids
             , from_ids
             , to_ids )
    {
        try
        {
            let data = [];
            
            for( i in from_ids )
            {
                data.push( { id: edge_ids[ i ]
                           , from: from_ids[ i ]
                           , label: to_ids[ i ] } );
            }

            this.network.body.data.edges.add( data );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    center_viewport_node( node_id )
    {
        try
        {
            this.redraw();

            console.assert( this.network );

            let pos = this.network.getPositions( [ node_id ] )[ node_id ];

            console.assert( pos );

            this.network.moveTo( { position: { x: pos.x
                                             , y: pos.y } } );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    change_node_font( uuid
                    , new_face
                    , new_color )
    {
        try
        {
            let n = this.network.body.data.nodes.get( uuid );

            n.font = { color: new_color
                     , face: new_face };

            this.network.body.data.nodes.update( n );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    children_ids( node_id )
    {
        try
        {
            return this.network.getConnectedNodes( node_id, 'to' );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    color_node_background( uuid
                         , color )
    {
        try
        {
            let n = this.network.body.data.nodes.get( uuid );

            n.color = { background: color
                      , highlight: { background: color } };

            this.network.body.data.nodes.update( n );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    color_node_border( uuid
                     , color )
    {
        try
        {
            let n = this.network.body.data.nodes.get( uuid );

            n.color = { border: color
                      , background: '#f0f0f0' 
                      , highlight: { border: color
                                   , background: '#f0f0f0' } };

            this.network.body.data.nodes.update( n );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    // TODO: Ensure that from_ids.size() == to_ids.size()
    create_edges( edge_ids
                , from_ids
                , to_ids )
    {
        try
        {
            let data = [];
            
            for( i in from_ids )
            {
                data.push( { id: edge_ids[ i ]
                           , from: from_ids[ i ]
                           , to: to_ids[ i ] } );
            }

            this.network.body.data.edges.add( data );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    create_node( uuid
               , title )
    {
        try
        {
            this.network.body.data.nodes.add( { id: uuid
                                              , label: title } );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    // TODO: Ensure that uuids.size() == titles.size()
    create_nodes( uuids
                , titles )
    {
        try
        {
            let data = [];
            
            for( i in uuids )
            {
                data.push( { id: uuids[ i ]
                           , label: titles[ i ] } );
            }

            this.network.body.data.nodes.add( data );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    destroy_network()
    {
        try
        {
            this.network.destroy();
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    edge_exists( edge_id )
    {
        try
        {
            return this.network.body.data.edges.get( edge_id ) !== null;
        }
        catch( err )
        {
            console.log( String( err ) );
            throw err;
        }
    }

    focus_network()
    {
        try
        {
            this.network.canvas.frame.focus();
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    id_exists( id )
    {
        try
        {
            let ids = this.network.body.data.nodes.getIds();

            return ids.indexOf( id ) != -1;
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    node_ids()
    {
        try
        {
            return this.network.body.data.nodes.getIds();
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    node_position( node_id )
    {
        try
        {
            let pos = this.network.getPositions( [ node_id ] )[ node_id ];

            return pos;
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    parent_id( node_id )
    {
        try
        {
            return this.network.getConnectedNodes( node_id, 'from' )[ 0 ];
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    redraw()
    {
        try
        {
            this.network.redraw();
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    remove_edge( edge_id )
    {
        try
        {
            this.network.body.data.edges.remove( edge_id );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    remove_edges()
    {
        try
        {
            this.network.body.data.edges.clear();
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    remove_node( nid )
    {
        try
        {
            // TODO: Not an efficient way to remove edges (iterating over all edges), but given small number displayed typically, not much of a concern.
            this.network.body.data.edges.forEach( function( e )
            {
                if( e.from === nid
                 || e.to === nid )
                {
                    this.network.body.data.edges.remove( e.id );
                }
            }.bind(this) );

            this.network.body.data.nodes.remove( { id: nid } );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    remove_nodes()
    {
        try
        {
            this.remove_edges();

            this.network.body.data.nodes.clear();
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    select_node( node_id )
    {
        try
        {
            this.network.setSelection( { nodes: [ node_id ]
                                       , edges: [] } );

            return true;
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }
    
    selected_node_id()
    {
        try
        {
            let ns = this.network.getSelectedNodes();

            if( ns.length != 1 )
            {
                return null;
            }

            return ns[ 0 ];
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    scale_viewport( zoom_scale )
    {
        try
        {
            this.network.moveTo( { scale: zoom_scale } );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }
    viewport_scale()
    {
        try
        {
            return this.network.getScale();
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    title_of( id )
    {
        try
        {
            return this.network.body.data.nodes.get( id ).label;
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }

    update_title( id
                , title )
    {
        try
        {
            this.network.body.data.nodes.update( { id: id
                                                 , label: title } );
        }
        catch( err )
        {
            console.error( String( err ) );
            throw err;
        }
    }
}

function new_network( container_id )
{
    try
    {
        // TODO: Do better error checking than assertions.
        const container_id_s = kmap.uuid_to_string( container_id ); console.assert( container_id_s.has_value() );
        const elem_id = document.getElementById( container_id_s.value() ); console.assert( elem_id );

        return new Network( elem_id );
    }
    catch( err )
    {
        console.error( String( err ) );
        throw err;
    }
}

let network_on_deselect_node_event = function( network ) 
{
    try
    {
        return function( e )
        {
            if( e.nodes.length == 0 ) // New node wasn't selected.
            {
                // So, reselect the former.
                const prev_sel = e.previousSelection.nodes[ 0 ];
                // Note: double "select node" is non-straightforward workaround. Here's what I understand to be going on:
                // - kmap.select_node() assumes that the network always has a node selected, so if it's called without
                //   visjs having a node selected, it will err - even though it proceeds to call select_node() itslef.
                network.select_node( prev_sel );
                kmap.select_node( kmap.uuid_from_string( prev_sel ).value() ).throw_on_error(); // Assumes 'prev_sel' valid UUID.
            }
        };
    }
    catch( err )
    {
        console.error( String( err ) );
        throw err;
    }
};

let network_on_select_node_event = function( network ) 
{
    try
    {
        return function( e )
        {
            if( e.nodes.length != 0 )
            {
                const node = e.nodes[ 0 ];
                // Note: double "select node" is non-straightforward workaround. Here's what I understand to be going on:
                // - kmap.select_node() assumes that the network always has a node selected, so if it's called without
                //   visjs having a node selected, it will err - even though it proceeds to call select_node() itslef.
                // network.select_node( node );
                kmap.select_node( kmap.uuid_from_string( node ).value() ).throw_on_error(); // Assumes 'node' valid UUID.
            }
        };
    }
    catch( err )
    {
        console.error( String( err ) );
        throw err;
    }
};

function network_onkeydown( e )
{
    if( e.type !== "keydown" )
    {
        const err_msg = "expected 'keydown': " + e.type;
        console.error( err_msg );
        throw err_msg;
    }

    const mnemonic = e.key.toLowerCase();
    const valid_keys =
        [ 
          'arrowdown'
        , 'arrowleft'
        , 'arrowright'
        , 'arrowup'
        , 'c'
        , 'control'
        , 'e'
        , 'esc'
        , 'g'
        , 'h'
        , 'i'
        , 'j'
        , 'k'
        , 'l'
        , 'o'
        , 'p'
        , 'shift'
        , 'v'
        ];

    if( valid_keys.includes( mnemonic ) )
    {
        kmap.event_store().fire_event( to_VectorString( [ 'subject.network', 'verb.depressed', 'object.keyboard.key.' + mnemonic ] ) ).throw_on_error();
        e.preventDefault();
    }
}

function network_onkeyup( e )
{
    if( e.type !== "keyup" )
    {
        const err_msg = "expected 'keyup': " + e.type;
        console.error( err_msg );
        throw err_msg;
    }

    const mnemonic = e.key.toLowerCase();
    const valid_keys =
        [ 
          'arrowdown'
        , 'arrowleft'
        , 'arrowright'
        , 'arrowup'
        , 'c'
        , 'control'
        , 'e'
        , 'esc'
        , 'g'
        , 'h'
        , 'i'
        , 'j'
        , 'k'
        , 'l'
        , 'o'
        , 'p'
        , 'shift'
        , 'v'
        ];

    if( valid_keys.includes( mnemonic ) )
    {
        kmap.event_store().fire_event( to_VectorString( [ 'subject.network', 'verb.raised', 'object.keyboard.key.' + mnemonic ] ) ).throw_on_error();
        e.preventDefault();
    }
}
