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
                                                    , maximum: 200 } } };
            this.network = new vis.Network( container
                                          , data
                                          , options );
            
            this.network.canvas.frame.onkeydown = network_onkeydown;
            this.network.on( 'deselectNode', network_on_deselect_node_event( this ) );
            this.network.on( 'selectNode', network_on_select_node_event( this ) );
        }
        catch( err )
        {
            console.log( String( err ) );
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
            console.log( String( err ) );
        }
    }

    center_viewport_node( node_id )
    {
        try
        {
            {
                // Note:
                //   This is a bit of a hack. Even with `autoResize: true`, it appears that there is a delay as to when the redraw happens, or race condition.
                //   In any case, when a canvas change occurs, followed by a call to this (director or e.g., through select_node), the network may not have yet been
                //   redrawn; thus, force the redraw here.
                // TODO:
                //   It would be more appropriate to disable `autoResize`, and use the `window.onresize` callback to dispatch to kmap's event handler - which, of course,
                //   the Network would register a callback on resize that manually forces `this.network.redraw()`. Presumably, there wouldn't be the same race condition that this is meant to fix.
                //   We want to pull the zoom scale from a setting, but what if the user changes zoom? I could have an option.network.zoom.[default|present] to store and pull from the last.
                //   Of course, will require event handler on zoom for visjs network.
                this.set_zoom( 1.0 );
                this.redraw();
            }

            let pos = this.network.getPositions( [ node_id ] )[ node_id ];

            this.network.moveTo( { position: { x: pos.x
                                             , y: pos.y } } );
        }
        catch( err )
        {
            console.log( String( err ) );
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
            console.log( String( err ) );
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
            console.log( String( err ) );
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
            console.log( String( err ) );
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
            console.log( String( err ) );
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
            console.log( String( err ) );
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
            console.log( String( err ) );
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
            console.log( String( err ) );
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
            console.log( String( err ) );
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
            console.log( String( err ) );
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
            console.log( String( err ) );
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
            console.log( String( err ) );
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
            console.log( String( err ) );
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
            console.log( String( err ) );
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
            console.log( String( err ) );
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
            console.log( String( err ) );
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
            console.log( String( err ) );
        }
    }

    remove_node( nid )
    {
        try
        {
            this.network.body.data.nodes.remove( { id: nid } );
            this.network.body.data.edges.forEach( function( e )
            {
                if( e.from == nid
                || e.to == nid )
                {
                    this.network.body.data.edges.remove( e.id );
                }
            } );
        }
        catch( err )
        {
            console.log( String( err ) );
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
            console.log( String( err ) );
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
            console.log( String( err ) );
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
            console.log( String( err ) );
        }
    }

    set_zoom( zoom_scale )
    {
        try
        {
            this.network.moveTo( { scale: zoom_scale } )
        }
        catch( err )
        {
            console.log( String( err ) );
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
            console.log( String( err ) );
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
            console.log( String( err ) );
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
        console.log( String( err ) );
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
        console.log( String( err ) );
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
                network.select_node( node );
                kmap.select_node( kmap.uuid_from_string( node ).value() ).throw_on_error(); // Assumes 'node' valid UUID.
            }
        };
    }
    catch( err )
    {
        console.log( String( err ) );
    }
};

function network_onkeydown( e )
{
    try
    {
        let key = e.keyCode ? e.keyCode : e.which;
        let is_ctrl = !!e.ctrlKey;
        let is_shift = !!e.shiftKey;

        switch ( key )
        {
            case 69: // e
            {
                kmap.parse_cli( ':edit.body' );
                e.preventDefault();
                break;
            }
            case 72: // h
            case 37: // left-arrow
            {
                kmap.travel_left().throw_on_error();
                e.preventDefault();
                break;
            }
            case 74: // j
            case 40: // down-arrow
            {
                kmap.travel_down().throw_on_error();
                e.preventDefault();
                break;
            }
            case 75: // k
            case 38: // up-arrow
            {
                kmap.travel_up().throw_on_error();
                e.preventDefault();
                break;
            }
            case 76: // l
            case 39: // right-arrow
            {
                kmap.travel_right().throw_on_error();
                e.preventDefault();
                break;
            }
            case 73: // i
            {
                if( is_ctrl )
                {
                    console.log( 'ctrl+i' );
                    kmap.jump_in();
                }
                break;
            }
            case 79: // o
            {
                if( is_ctrl )
                {
                    console.log( 'ctrl+o' );
                    kmap.jump_out();
                }
                break;
            }
            case 86: // v
            {
                kmap.view_body();
                e.preventDefault();
                break;
            }
            case 71: // g
            {
                if( is_shift ) // G
                {
                    kmap.travel_bottom().throw_on_error();
                }
            }
        }
    }
    catch( err )
    {
        console.log( String( err ) );
    }
}
